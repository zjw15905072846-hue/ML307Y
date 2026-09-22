#ifndef __CM_DEMO_AUDIO_H__
#define __CM_DEMO_AUDIO_H__

#include <stdint.h>
#include <stddef.h>
#include "embedded_cli.h"

void cm_test_audio_play(EmbeddedCli *cli, char *args, void *context);
void cm_test_audio_record(EmbeddedCli *cli, char *args, void *context);
#endif