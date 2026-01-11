#include "client/bg_jobs.h"

#include "client/net_client.h"
#include "common/protocol.h"
#include "common/io.h"
#include "common/error.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct bg_job_args {
  struct client_state state;
  char path1[1024];
  char path2[1024];
  int is_upload;
};

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_pending = 0;

int bg_jobs_init(void) {
  g_pending = 0;
  return 0;
}

int bg_jobs_pending(void) {
  pthread_mutex_lock(&g_mu);
  int n = g_pending;
  pthread_mutex_unlock(&g_mu);
  return n;
}

static void pending_inc(void) {
  pthread_mutex_lock(&g_mu);
  g_pending++;
  pthread_mutex_unlock(&g_mu);
}

static void pending_dec(void) {
  pthread_mutex_lock(&g_mu);
  if (g_pending > 0) {
    g_pending--;
  }
  pthread_mutex_unlock(&g_mu);
}

static int send_login(int fd, const char *user) {
  if (sendf_line(fd, "login %s", user) != 0) {
    return -1;
  }
  char line[256];
  if (recv_line(fd, line, sizeof(line)) <= 0) {
    return -1;
  }
  return (strncmp(line, "OK", 2) == 0) ? 0 : -1;
}

static void *bg_thread(void *arg) {
  struct bg_job_args *job = (struct bg_job_args *)arg;

  int fd = connect_to_server(job->state.cfg.ip, job->state.cfg.port);
  if (fd < 0) {
    fprintf(stdout, "[Background] Command failed: connection\n");
    fflush(stdout);
    pending_dec();
    free(job);
    return NULL;
  }

  if (send_login(fd, job->state.user) != 0) {
    fprintf(stdout, "[Background] Command failed: login\n");
    fflush(stdout);
    close(fd);
    pending_dec();
    free(job);
    return NULL;
  }

  if (job->is_upload) {
    FILE *in = fopen(job->path1, "rb");
    if (!in) {
      fprintf(stdout, "[Background] Command failed: upload\n");
      fflush(stdout);
      close(fd);
      pending_dec();
      free(job);
      return NULL;
    }
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);

    sendf_line(fd, "upload %s %ld", job->path2, size);
    char buf[4096];
    long remaining = size;
    while (remaining > 0) {
      size_t n = fread(buf, 1, sizeof(buf), in);
      if (n == 0) {
        break;
      }
      send_blob(fd, buf, n);
      remaining -= (long)n;
    }
    fclose(in);
    char line[256];
    if (recv_line(fd, line, sizeof(line)) > 0 && strncmp(line, "OK", 2) == 0) {
      fprintf(stdout, "[Background] Command: upload %s %s concluded\n", job->path2, job->path1);
      fflush(stdout);
    } else {
      fprintf(stdout, "[Background] Command failed: upload\n");
      fflush(stdout);
    }
  } else {
    char line[256];
    int attempts = 40;
    int ok = 0;
    while (attempts-- > 0) {
      sendf_line(fd, "download %s", job->path1);
      if (recv_line(fd, line, sizeof(line)) <= 0) {
        break;
      }
      if (strncmp(line, "OK", 2) == 0) {
        ok = 1;
        break;
      }
      int code = -1;
      if (sscanf(line, "ERR %d", &code) == 1 &&
          (code == ERR_NOT_FOUND || code == ERR_PERM)) {
        struct timespec req = {.tv_sec = 0, .tv_nsec = 100000000};
        nanosleep(&req, NULL);
        continue;
      }
      break;
    }
    if (ok) {
      long size = 0;
      sscanf(line, "OK %ld", &size);
      FILE *out = fopen(job->path2, "wb");
      if (out) {
        char buf[4096];
        long remaining = size;
        while (remaining > 0) {
          size_t chunk = remaining > (long)sizeof(buf) ? sizeof(buf) : (size_t)remaining;
          if (recv_blob(fd, buf, chunk) != 0) {
            break;
          }
          fwrite(buf, 1, chunk, out);
          remaining -= (long)chunk;
        }
        fclose(out);
        fprintf(stdout, "[Background] Command: download %s %s concluded\n", job->path1, job->path2);
        fflush(stdout);
      } else {
        fprintf(stdout, "[Background] Command failed: download\n");
        fflush(stdout);
      }
    } else {
      fprintf(stdout, "[Background] Command failed: download\n");
      fflush(stdout);
    }
  }

  close(fd);
  pending_dec();
  free(job);
  return NULL;
}

static int start_job(const struct client_state *state, const char *p1, const char *p2, int is_upload) {
  if (!state || !state->logged_in) {
    return -1;
  }
  struct bg_job_args *job = malloc(sizeof(*job));
  if (!job) {
    return -1;
  }
  memset(job, 0, sizeof(*job));
  job->state = *state;
  snprintf(job->path1, sizeof(job->path1), "%s", p1);
  snprintf(job->path2, sizeof(job->path2), "%s", p2);
  job->is_upload = is_upload;

  pthread_t tid;
  pending_inc();
  if (pthread_create(&tid, NULL, bg_thread, job) != 0) {
    pending_dec();
    free(job);
    return -1;
  }
  pthread_detach(tid);
  return 0;
}

int bg_start_upload(const struct client_state *state, const char *local_path, const char *remote_path) {
  return start_job(state, local_path, remote_path, 1);
}

int bg_start_download(const struct client_state *state, const char *remote_path, const char *local_path) {
  return start_job(state, remote_path, local_path, 0);
}
