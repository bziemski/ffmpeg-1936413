/*
 * Copyright (c) 2018 Marton Balint
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with FFmpeg; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "avfilter.h"
#include "filters.h"
#include "internal.h"
#include <time.h>

typedef struct CueContext {
    const AVClass *class;
    int64_t first_pts;
    int64_t cue;
    int64_t first_timestamp;
    int64_t start_position;
    int output_started;
    int first_coded_picture_number;
    int first_display_picture_number;
    int init;
    int buffered_frames;
} CueContext;

static int activate(AVFilterContext *ctx)
{
    AVFilterLink *inlink = ctx->inputs[0];
    AVFilterLink *outlink = ctx->outputs[0];
    CueContext *s = ctx->priv;
    FF_FILTER_FORWARD_STATUS_BACK(outlink, inlink);
    
    if(!s->init){
        s->start_position = s->first_timestamp * inlink->time_base.den / inlink->time_base.num / 1000000;
        s->init = 1;
    }
    
    if (ff_inlink_queued_frames(inlink)) {
        
        AVFrame *frame;
        frame = ff_inlink_peek_frame(inlink, ff_inlink_queued_frames(inlink) - 1);
        av_log(ctx, AV_LOG_DEBUG, "Original frame pts= %ld %p \n", frame->pts, frame);
       
        if(frame->pts < s->start_position && !s->output_started){
            av_log(ctx, AV_LOG_INFO, "Got frame pts=%ld Waiting for %ld\n", frame->pts, s->start_position);
            int64_t peeked_frame = frame->pts;
            int ret2 = ff_inlink_consume_frame(inlink, &frame);
            if(ret2<0){
                av_log(ctx, AV_LOG_ERROR, "Consume failed \n");
            }
            av_log(ctx, AV_LOG_DEBUG, "Discarded frame pts=%ld index=%d\n", frame->pts, frame->coded_picture_number);
            if(peeked_frame != frame->pts){
                av_log(ctx, AV_LOG_WARNING, "Discarded wrong frame! Peeked frame pts %ld and discarded %ld \n", peeked_frame, frame->pts);
            }
        }
        else{
            if(!s->output_started && s->buffered_frames == 0 && av_gettime() >= s->cue){
                av_log(ctx, AV_LOG_ERROR, "No frames buffered. Video can play delayed! Time behind: %ld Will try to catch...\n", (av_gettime()-s->cue));
                s->cue += 1000000 * inlink->frame_rate.den / inlink->frame_rate.num;
                s->start_position += inlink->time_base.den * inlink->frame_rate.den / inlink->frame_rate.num / inlink->time_base.num;
                int ret2 = ff_inlink_consume_frame(inlink, &frame);
                if(ret2<0){
                    av_log(ctx, AV_LOG_ERROR, "Consume failed \n");
                }
            }
            else{
                if(!s->first_coded_picture_number){
                    s->first_coded_picture_number = frame->coded_picture_number;
                }

                // TODO: Handle B-Frames!
                if(!s->first_display_picture_number){
                    s->first_display_picture_number = frame->display_picture_number;
                } 
                if(!s->first_pts){
                    s->first_pts = frame->pts;
                }

                av_log(ctx, AV_LOG_DEBUG, "Original frame pts= %ld first_pts = %ld pkt_pos = %ld best_effort_timestamp = %ld pkt_dts = %ld coded_picture_number = %d display_picture_number = %d\n", frame->pts, s->first_pts, frame->pkt_pos, frame->best_effort_timestamp, frame->pkt_dts, frame->coded_picture_number, frame->display_picture_number);
                double frame_len_s = 1000000 *inlink->frame_rate.den / inlink->frame_rate.num;

                if(av_gettime() >= s->cue - frame_len_s *  10 && !s->output_started){
                        av_log(ctx, AV_LOG_INFO, "Waiting for cue... \n");
                        int64_t diff;
                        while ((diff = (av_gettime() - s->cue)) < 0)
                            av_usleep(av_clip(-diff / 2, 100, 1000000));
                }

                if (av_gettime() >= s->cue){ 
                    int ret2 = ff_inlink_consume_frame(inlink, &frame);  //Takes first frame from the buffer 
                    if(ret2<0){
                        av_log(ctx, AV_LOG_ERROR, "Consume failed \n");
                    }
                   
                    if(!s->output_started){
                        s->output_started = 1;
                    }

                    av_log(ctx, AV_LOG_DEBUG, "Buffered frames: %d   queued: %zu \n", s->buffered_frames, ff_inlink_queued_frames(inlink));
                    frame->coded_picture_number -= s->first_coded_picture_number; 
                    frame->display_picture_number -= s->first_display_picture_number;
                    frame->pts -= s->first_pts;

                    return ff_filter_frame(outlink, frame);
                }
                else{
                    s->buffered_frames ++;
                    av_log(ctx, AV_LOG_DEBUG, "Buffered frames: %d \n", s->buffered_frames);
                }
            }
        }
    }

    FF_FILTER_FORWARD_STATUS(inlink, outlink);
    FF_FILTER_FORWARD_WANTED(outlink, inlink);

    return FFERROR_NOT_READY;
}

#define OFFSET(x) offsetof(CueContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_FILTERING_PARAM
static const AVOption options[] = {
    { "cue", "cue unix timestamp in microseconds", OFFSET(cue), AV_OPT_TYPE_INT64, { .i64 = 0 }, 0, INT64_MAX, FLAGS },
    { "first_timestamp", "Discards frames with a timestamp smaller than first_timestamp in microseconds", OFFSET(first_timestamp), AV_OPT_TYPE_INT64, { .i64 = 0 }, 0, INT64_MAX, FLAGS },
    { NULL }
};

#if CONFIG_CUE_FILTER
#define cue_options options
AVFILTER_DEFINE_CLASS(cue);

static const AVFilterPad cue_inputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

static const AVFilterPad cue_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

AVFilter ff_vf_cue = {
    .name        = "cue",
    .description = NULL_IF_CONFIG_SMALL("Delay filtering to match a cue."),
    .priv_size   = sizeof(CueContext),
    .priv_class  = &cue_class,
    .inputs      = cue_inputs,
    .outputs     = cue_outputs,
    .activate    = activate,
};
#endif /* CONFIG_CUE_FILTER */

#if CONFIG_ACUE_FILTER
#define acue_options options
AVFILTER_DEFINE_CLASS(acue);

static const AVFilterPad acue_inputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_AUDIO,
    },
    { NULL }
};

static const AVFilterPad acue_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_AUDIO,
    },
    { NULL }
};

AVFilter ff_af_acue = {
    .name        = "acue",
    .description = NULL_IF_CONFIG_SMALL("Delay filtering to match a cue."),
    .priv_size   = sizeof(CueContext),
    .priv_class  = &acue_class,
    .inputs      = acue_inputs,
    .outputs     = acue_outputs,
    .activate    = activate,
};
#endif /* CONFIG_ACUE_FILTER */
