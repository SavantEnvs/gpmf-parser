#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "GPMF_parser.h"
#include "GPMF_utils.h"

/* Raw GPMF payload (the bytes the MP4 'gpmd' track carries), driven the way demo/GPMF_demo.c does. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 8 || size > (1u << 20))
		return -1;

	uint32_t *payload = (uint32_t *)malloc(size);
	if (!payload)
		return 0;
	memcpy(payload, data, size);

	GPMF_stream metadata_stream = { 0 }, *ms = &metadata_stream;
	if (GPMF_OK == GPMF_Init(ms, payload, (uint32_t)size))
	{
		GPMF_Validate(ms, GPMF_RECURSE_LEVELS);
		GPMF_ResetState(ms);

		GPMF_ERR nextret;
		int guard = 0;
		do
		{
			(void)GPMF_Key(ms);
			(void)GPMF_NestLevel(ms);
			nextret = GPMF_Next(ms, GPMF_RECURSE_LEVELS | GPMF_TOLERANT);
			while (nextret == GPMF_ERROR_UNKNOWN_TYPE && guard++ < 100000)
				nextret = GPMF_Next(ms, GPMF_RECURSE_LEVELS);
		} while (GPMF_OK == nextret && guard++ < 100000);
		GPMF_ResetState(ms);

		while (GPMF_OK == GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS | GPMF_TOLERANT))
		{
			if (GPMF_OK != GPMF_SeekToSamples(ms))
				continue;

			void *raw = GPMF_RawData(ms);
			uint32_t samples = GPMF_Repeat(ms);
			uint32_t elements = GPMF_ElementsInStruct(ms);
			(void)GPMF_PayloadSampleCount(ms);
			(void)GPMF_StructSize(ms);
			(void)GPMF_RawDataSize(ms);
			(void)GPMF_FormattedDataSize(ms);

			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);
			char devname[64];
			GPMF_DeviceName(&find_stream, devname, sizeof(devname));
			(void)GPMF_DeviceID(&find_stream);
			GPMF_CopyState(ms, &find_stream);
			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL | GPMF_TOLERANT))
			{
				char expanded[64];
				uint32_t dstsize = sizeof(expanded);
				GPMF_ExpandComplexTYPE((char *)GPMF_RawData(&find_stream), GPMF_RawDataSize(&find_stream), expanded, &dstsize);
			}

			if (raw && samples && elements && samples < (1u << 16) && elements < 256)
			{
				uint32_t buffersize = samples * elements * (uint32_t)sizeof(double);
				double *tmp = (double *)malloc(buffersize);
				if (tmp)
				{
					GPMF_ScaledData(ms, tmp, buffersize, 0, samples, GPMF_TYPE_DOUBLE);
					GPMF_FormattedData(ms, tmp, buffersize, 0, samples);
					free(tmp);
				}
			}
		}
		GPMF_ResetState(ms);
		GPMF_Free(ms);
	}
	free(payload);
	return 0;
}
