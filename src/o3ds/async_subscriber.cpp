
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include "async_subscriber.h"
namespace O3DS
{


AsyncSubscriber::AsyncSubscriber()
{}

bool AsyncSubscriber::connect(const char *url)
{
	int ret;

	ret = nng_sub0_open(&mSocket);
	if (ret != 0)
	{
		mError = std::string("nng_req0_open: %s\n") + nng_strerror(ret);
		return false;
	}

	ret = nng_dial(mSocket, url, NULL, 0);
	if (ret != 0)
	{
		mError = std::string("nng_dial: %s\n") + nng_strerror(ret);
		return false;
	}

	ret = nng_setopt(mSocket, NNG_OPT_SUB_SUBSCRIBE, "", 0);
	if (ret != 0) { return false; }

	ret = nng_aio_alloc(&aio, AsyncSubscriber::Callback, this);
	if (ret != 0)
	{
		mError = std::string("nng_aio_alloc: %s\n") + nng_strerror(ret);
		return false;
	}

	ret = nng_ctx_open(&ctx, mSocket);
	if (ret != 0)
	{
		mError = std::string("nng_ctx_open: %s\n") + nng_strerror(ret);
		return false;
	}
	return true;
}

void AsyncSubscriber::Callback_()
{
	nng_msg *msg;

	int ret;

	ret = nng_aio_result(aio);
	if (ret != 0) return;

	msg = nng_aio_get_msg(aio);

	size_t len = nng_msg_len(msg);
	if (len == 0) return;

	void *data = nng_msg_body(msg);

	in_data((const char*)data, len);

	nng_msg_free(msg);

	nng_ctx_recv(ctx, aio);
}

}