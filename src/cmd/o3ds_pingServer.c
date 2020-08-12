// Based mainly on https://github.com/nanomsg/nng/blob/master/demo/async/server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/supplemental/util/platform.h>

// Parallel is the maximum number of outstanding requests we can handle.
// This is *NOT* the number of threads in use, but instead represents
// outstanding work items.  Select a small number to reduce memory size.
// (Each one of these can be thought of as a request-reply loop.)  Note
// that you will probably run into limitations on the number of open file
// descriptors if you set this too high. (If not for that limit, this could
// be set in the thousands, each context consumes a couple of KB.)
#ifndef PARALLEL
#define PARALLEL 128
#endif

// The server keeps a list of work items, sorted by expiration time,
// so that we can use this to set the timeout to the correct value for
// use in poll.
struct work {
  enum { INIT, RECV, WAIT, SEND } state;
  nng_aio *aio;
  nng_msg *msg;
  nng_ctx  ctx;
};

void
fatal(const char *func, int rv)
{
  fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
  exit(1);
}

void
server_cb(void *arg)
{
  struct work *work = arg;
  nng_msg *    msg;
  int          rv;
  uint32_t     when;

  switch (work->state) {
  case INIT:
    work->state = RECV;
    nng_ctx_recv(work->ctx, work->aio);
    break;
  case RECV:
    if ((rv = nng_aio_result(work->aio)) != 0) {
      fatal("nng_ctx_recv", rv);
    }
    msg = nng_aio_get_msg(work->aio);
    work->msg   = msg;	
	printf("Message  %zu\n", nng_msg_len(msg));
    work->state = WAIT;
    //nng_sleep_aio(when, work->aio);
    break;
  case WAIT:
    // We could add more data to the message here.
    nng_aio_set_msg(work->aio, work->msg);
    work->msg   = NULL;
    work->state = SEND;
	printf("Wait  %zu\n", nng_msg_len(work->msg));
	nng_ctx_send(work->ctx, work->aio);
    break;
  case SEND:
    if ((rv = nng_aio_result(work->aio)) != 0) {
      nng_msg_free(work->msg);
      fatal("nng_ctx_send", rv);
    }
	printf("Send  %zu\n", nng_msg_len(work->msg));
    work->state = RECV;
    nng_ctx_recv(work->ctx, work->aio);
    break;
  default:
    fatal("bad state!", NNG_ESTATE);
    break;
  }
}

struct work *
alloc_work(nng_socket sock)
{
  struct work *w;
  int          rv;

  if ((w = nng_alloc(sizeof(*w))) == NULL) {
    fatal("nng_alloc", NNG_ENOMEM);
  }
  if ((rv = nng_aio_alloc(&w->aio, server_cb, w)) != 0) {
    fatal("nng_aio_alloc", rv);
  }
  if ((rv = nng_ctx_open(&w->ctx, sock)) != 0) {
    fatal("nng_ctx_open", rv);
  }
  w->state = INIT;
  return (w);
}

// The server runs forever.
int
server(const char *url)
{
  nng_socket   sock;
  struct work *works[PARALLEL];
  int          rv;
  int          i;

  /*  Create the socket. */
  rv = nng_rep0_open(&sock);
  if (rv != 0) {
    fatal("nng_rep0_open", rv);
  }

  for (i = 0; i < PARALLEL; i++) {
    works[i] = alloc_work(sock);
  }

  if ((rv = nng_listen(sock, url, NULL, 0)) != 0) {
    fatal("nng_listen", rv);
  }

  for (i = 0; i < PARALLEL; i++) {
    server_cb(works[i]); // this starts them going (INIT state)
  }

  for (;;) {
    nng_msleep(3600000); // neither pause() nor sleep() portable
  }
}

int
main(int argc, char **argv)
{
  int rc;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <url>\n", argv[0]);
    fprintf(stderr, "Example: %s tcp://127.0.0.1:5555\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  rc = server(argv[1]);
  exit(rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
