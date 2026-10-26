#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "GPMF_parser.h"
#include "demo/GPMF_mp4reader.h"

/* An MP4/MOV file, driven the way demo/GPMF_demo.c readMP4File() does (mp4reader + payload walk). */
static char path[] = "/tmp/gpmfdemo-fuzz-XXXXXX";
static int fd = -1;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 16 || size > (4u << 20))
		return -1;

	if (fd < 0)
	{
		fd = mkstemp(path);
		if (fd < 0)
			return 0;
	}
	if (ftruncate(fd, 0) != 0 || pwrite(fd, data, size, 0) != (ssize_t)size)
		return 0;

	size_t mp4handle = OpenMP4Source(path, MOV_GPMF_TRAK_TYPE, MOV_GPMF_TRAK_SUBTYPE, 0);
	if (mp4handle == 0)
		return 0;

	if (GetDuration(mp4handle) > 0.0)
	{
		uint32_t fr_num, fr_dem;
		(void)GetVideoFrameRateAndCount(mp4handle, &fr_num, &fr_dem);

		uint32_t payloads = GetNumberPayloads(mp4handle);
		size_t payloadres = 0;
		for (uint32_t index = 0; index < payloads && index < 64; index++)
		{
			double in = 0.0, out = 0.0;
			uint32_t payloadsize = GetPayloadSize(mp4handle, index);
			if (payloadsize > (1u << 24))
				break;
			payloadres = GetPayloadResource(mp4handle, payloadres, payloadsize);
			uint32_t *payload = GetPayload(mp4handle, payloadres, index);
			if (payload == NULL)
				break;
			if (GetPayloadTime(mp4handle, index, &in, &out) != GPMF_OK)
				break;

			GPMF_stream metadata_stream = { 0 }, *ms = &metadata_stream;
			if (GPMF_OK != GPMF_Init(ms, payload, payloadsize))
				break;
			GPMF_Validate(ms, GPMF_RECURSE_LEVELS);
			GPMF_ResetState(ms);

			GPMF_ERR nextret;
			int guard = 0;
			do
			{
				(void)GPMF_Key(ms);
				nextret = GPMF_Next(ms, GPMF_RECURSE_LEVELS | GPMF_TOLERANT);
				while (nextret == GPMF_ERROR_UNKNOWN_TYPE && guard++ < 100000)
					nextret = GPMF_Next(ms, GPMF_RECURSE_LEVELS);
			} while (GPMF_OK == nextret && guard++ < 100000);
			GPMF_ResetState(ms);
		}
		if (payloadres)
			FreePayloadResource(mp4handle, payloadres);
	}
	CloseSource(mp4handle);
	return 0;
}
