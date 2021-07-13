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
#include "libavutil/nw_log.h"
#include <time.h>

typedef struct CueContext {
    const AVClass *class;
    int64_t first_pts;
    int64_t cue;
    int64_t preroll;
    int64_t buffer;
    int status;
} CueContext;

static void to_date(int64_t ts, char* res){
    time_t t = ts/1000000;
    
    const char* format = "%H:%M:%S";
    struct tm lt;
    (void) localtime_r(&t, &lt);
    if (strftime(res, 64, format, &lt) == 0) {
                return;
    }
    return ;
}

static int activate(AVFilterContext *ctx)
{
    AVFilterLink *inlink = ctx->inputs[0];
    AVFilterLink *outlink = ctx->outputs[0];
    CueContext *s = ctx->priv;
    static int nw_dirty_state = 0;
    FF_FILTER_FORWARD_STATUS_BACK(outlink, inlink);
    static int coded_picture_number_base = 0;
    static int display_picture_number_base = 0;
    static int64_t pts_base = 0;
    
    int64_t cue_startpoint = s->cue - 1000000 * 1;
    if (ff_inlink_queued_frames(inlink)) {
        static int64_t last_invoke = 0;
        if(last_invoke != 0){
            av_log(ctx, AV_LOG_WARNING, "elapsed= %ld \n", av_gettime() - last_invoke);
        }
        last_invoke = av_gettime();
        // AVFrame *frame = ff_inlink_peek_frame(inlink, 0);
        AVFrame *frame;
        // int64_t pts = av_rescale_q(frame->pts, inlink->time_base, AV_TIME_BASE_Q);
        // char ts_buf[256];
        // get_nw_timestamp(ts_buf);
        char buf[64];
        to_date(cue_startpoint, buf);
        av_log(ctx, AV_LOG_WARNING, "cue_startpoint= %s \n", buf);
        char buf2[64];
        to_date(av_gettime(), buf2);
        av_log(ctx, AV_LOG_WARNING, "av_gettime= %s \n", buf2);
        if(av_gettime() >= cue_startpoint){
            frame = ff_inlink_peek_frame(inlink, ff_inlink_queued_frames(inlink) - 1);
            if(coded_picture_number_base == 0){
                coded_picture_number_base = frame->coded_picture_number;
            }
            if(display_picture_number_base == 0){
                display_picture_number_base = frame->display_picture_number;
            } 
            if(pts_base == 0){
                pts_base = frame->pts;
            }
            av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] Original frame pts= %ld pts_base = %ld pkt_pos = %ld best_effort_timestamp = %ld pkt_dts = %ld coded_picture_number = %d display_picture_number = %d\n", frame->pts, pts_base, frame->pkt_pos, frame->best_effort_timestamp, frame->pkt_dts, frame->coded_picture_number, frame->display_picture_number);
            frame->coded_picture_number -= coded_picture_number_base; 
            frame->display_picture_number -= display_picture_number_base;
            frame->pts -= pts_base;
            if (av_gettime() >= s->cue){
                int ret2 = ff_inlink_consume_frame(inlink, &frame);  //Takes first frame from the buffer 

                if(ret2<0){
                    av_log(ctx, AV_LOG_WARNING, "Consume failed \n");
                }
                // reading pts based on given segment index -> discurding everything till specific frame and displaying this frame in specific time

                av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] Returns frame pts= %ld  \n", frame->pts);

                return ff_filter_frame(outlink, frame);
            }
        }
        else{
            int ret2 = ff_inlink_consume_frame(inlink, &frame);
        
            if(ret2<0){
                av_log(ctx, AV_LOG_WARNING, "Consume failed \n");
            }
        }

        // av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] %ld %ld \n", s->cue, cue_startpoint);
        

        
        // if (!s->status) {
        //     char buf[64];
        //     to_date(s->cue, buf);
        //     av_log(ctx, AV_LOG_WARNING, "STATE 0 %s \n", buf);
        //     s->first_pts = pts;
        //     s->status++;
        // }
        // if (s->status == 1) {
        //     if(nw_dirty_state==0){
        //         char buf[64];
        //         to_date(s->cue, buf);
        //         av_log(ctx, AV_LOG_WARNING, "STATE 1 %s \n", buf);
        //     }
            
        //     if (pts - s->first_pts < s->preroll) {
        //         av_log(ctx, AV_LOG_WARNING, "consuming preroll frame: %d \n", frame->coded_picture_number);

        //         int ret = ff_inlink_consume_frame(inlink, &frame);
        //         if (ret < 0)
        //             return ret;
        //         return ff_filter_frame(outlink, frame);
        //     }
        //     s->first_pts = pts;
        //     s->status++;
        //     nw_dirty_state=1;
        // }
        // AVFrame *frame2;
        // if (s->status == 2) {
        //     if(nw_dirty_state==1){
        //         char buf[64];
        //         to_date(s->cue, buf);
        //         av_log(ctx, AV_LOG_WARNING, "STATE 2 %s \n", buf);
        //         av_log(ctx, AV_LOG_WARNING, "Buffer: %ld\n", s->buffer);
        //     }
        //     frame = ff_inlink_peek_frame(inlink, ff_inlink_queued_frames(inlink) - 1);
            
        //     int ret2 = ff_inlink_consume_frame(inlink, &frame2);
        //     if(ret2<0){
        //         av_log(ctx, AV_LOG_WARNING, "Consume failed \n");
        //     }
        //     pts = av_rescale_q(frame->pts, inlink->time_base, AV_TIME_BASE_Q);
        //     av_log(ctx, AV_LOG_WARNING, "pts: %ld first_pts: %ld \n", pts, s->first_pts);
        //     if (!(pts - s->first_pts < s->buffer && (av_gettime() - s->cue) < 0))
        //         s->status++;
        //     nw_dirty_state=2;
        // }
        // if (s->status == 3) {
        //     if(nw_dirty_state==2){
        //         char buf[64];
        //         to_date(s->cue, buf);
        //         av_log(ctx, AV_LOG_WARNING, "STATE 3 %s \n", buf);
        //     }
        //     int64_t diff;
        //     while ((diff = (av_gettime() - s->cue)) < 0)
        //         av_usleep(av_clip(-diff / 2, 100, 1000000));
        //     s->status++;
        //     nw_dirty_state=3;
        // }
        // if (s->status == 4) {
        //     if(nw_dirty_state==3){
        //         char buf[64];
        //         to_date(s->cue, buf);
        //         av_log(ctx, AV_LOG_WARNING, "STATE 4 %s \n", buf);
        //         pts = av_rescale_q(frame->pts, inlink->time_base, AV_TIME_BASE_Q);
        //         av_log(ctx, AV_LOG_WARNING, "Frame: %d pts: %ld\n", frame->coded_picture_number, pts);
        //         FF_FILTER_FORWARD_STATUS(inlink, outlink);
        //         FF_FILTER_FORWARD_WANTED(outlink, inlink);
        //     }
        //     else{
        //         int ret = ff_inlink_consume_frame(inlink, &frame2);
        //         if (ret < 0)
        //             return ret;
        //     }
            
        //     nw_dirty_state=4;
        //     return ff_filter_frame(outlink, frame2);
        // }
    }

    FF_FILTER_FORWARD_STATUS(inlink, outlink);
    FF_FILTER_FORWARD_WANTED(outlink, inlink);

    return FFERROR_NOT_READY;
}

#define OFFSET(x) offsetof(CueContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_FILTERING_PARAM
static const AVOption options[] = {
    { "cue", "cue unix timestamp in microseconds", OFFSET(cue), AV_OPT_TYPE_INT64, { .i64 = 0 }, 0, INT64_MAX, FLAGS },
    { "preroll", "preroll duration in seconds", OFFSET(preroll), AV_OPT_TYPE_DURATION, { .i64 = 0 }, 0, INT64_MAX, FLAGS },
    { "buffer", "buffer duration in seconds", OFFSET(buffer), AV_OPT_TYPE_DURATION, { .i64 = 0 }, 0, INT64_MAX, FLAGS },
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
