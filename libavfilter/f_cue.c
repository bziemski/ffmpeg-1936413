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

    static int64_t start_position = -2;

    static int output_started = 0;
    
    if(start_position == -2){
        start_position = s->buffer  * inlink->time_base.den / inlink->time_base.num / 1000000; //TODO: Check that
    }
    //TODO: If start_position is set to -1, just start forwarding frames at cue time


    static int buffered_frames = 0;
    
    if (ff_inlink_queued_frames(inlink)) {
        
        AVFrame *frame;
        AVFrame *frame3;

        av_log(ctx, AV_LOG_WARNING, "++++++++++++++++++++++++++++++++++++\n", buffered_frames);
        for(int i =0; i< ff_inlink_queued_frames(inlink); i++){
            frame3 = ff_inlink_peek_frame(inlink, i);
            av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] %ld %p\n", frame3->pts, frame3);
        }
    
        frame = ff_inlink_peek_frame(inlink, ff_inlink_queued_frames(inlink) - 1);
       
        av_log(ctx, AV_LOG_ERROR, "[NW_LOGGING] Original frame pts= %ld %p \n", frame->pts, frame);
                
       
        if(frame->pts < start_position && !output_started){
            av_log(ctx, AV_LOG_WARNING, "Got frame pts=%ld Waiting for %ld\n", frame->pts, start_position);
            int64_t peeked_frame = frame->pts;
            int ret2 = ff_inlink_consume_frame(inlink, &frame);
            if(ret2<0){
                av_log(ctx, AV_LOG_ERROR, "Consume failed \n");
            }
            av_log(ctx, AV_LOG_WARNING, "Discarded frame pts=%ld index=%d\n", frame->pts, frame->coded_picture_number);
            if(peeked_frame != frame->pts){
                av_log(ctx, AV_LOG_ERROR, "Discarded wrong frame! Peeked frame pts %ld and discarded %ld \n", peeked_frame, frame->pts);
            }
        }
        else{
            if(!output_started && buffered_frames == 0 && av_gettime() >= s->cue){
                av_log(ctx, AV_LOG_ERROR, "[NW_LOGGING] No frames buffered. Video can play delayed! Time behind: %ld Will try to catch...\n", (av_gettime()-s->cue));
                s->cue += 1000000 * inlink->frame_rate.den / inlink->frame_rate.num;
                start_position += inlink->time_base.den * inlink->frame_rate.den / inlink->frame_rate.num / inlink->time_base.num;
                int ret2 = ff_inlink_consume_frame(inlink, &frame);
                if(ret2<0){
                    av_log(ctx, AV_LOG_ERROR, "Consume failed \n");
                }
            }
            else{

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
                
                // frame->coded_picture_number -= coded_picture_number_base; 
                // frame->display_picture_number -= display_picture_number_base;
                // frame->pts -= pts_base;

                double frame_len_s = 1000000 *inlink->frame_rate.den / inlink->frame_rate.num;
                // av_log(ctx, AV_LOG_ERROR, "[NW_LOGGING] time: frame_len_s: %f %ld  cue_wait: %ld \n", frame_len_s, av_gettime(), s->cue - frame_len_s * 10);

                if(av_gettime() >= s->cue - frame_len_s *  10 && !output_started){
                        av_log(ctx, AV_LOG_WARNING, "Waiting for cue... \n");
                        int64_t diff;
                        while ((diff = (av_gettime() - s->cue)) < 0)
                            av_usleep(av_clip(-diff / 2, 100, 1000000));
                }

                if (av_gettime() >= s->cue){ 
                    int ret2 = ff_inlink_consume_frame(inlink, &frame);  //Takes first frame from the buffer 

                    if(ret2<0){
                        av_log(ctx, AV_LOG_ERROR, "Consume failed \n");
                    }
                   
                    av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] Returning frame pts= %ld  %d %d \n", frame->pts, inlink->time_base.num, inlink->time_base.den);
                    if(!output_started){
                        output_started = 1;
                    }
                    AVFrame *frame2;

                    av_log(ctx, AV_LOG_WARNING, "===============================\n", buffered_frames);
                    for(int i =0; i< ff_inlink_queued_frames(inlink); i++){
                        frame2 = ff_inlink_peek_frame(inlink, i);
                        av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] %ld %p\n", frame2->pts, frame2);

                    }
                    av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] buffered frames: %d   queued: %zu \n", buffered_frames,  ff_inlink_queued_frames(inlink));


                    frame->coded_picture_number -= coded_picture_number_base; 
                    frame->display_picture_number -= display_picture_number_base;
                    frame->pts -= pts_base;

                    return ff_filter_frame(outlink, frame);
                }
                else{
                    buffered_frames ++;
                    AVFrame *frame2;

                    av_log(ctx, AV_LOG_WARNING, "===============================\n", buffered_frames);
                    for(int i =0; i< ff_inlink_queued_frames(inlink); i++){
                        frame2 = ff_inlink_peek_frame(inlink, i);
                        av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] %ld %p\n", frame2->pts, frame2);

                    }
                    av_log(ctx, AV_LOG_WARNING, "[NW_LOGGING] buffered frames: %d \n", buffered_frames);
                }
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
    { "buffer", "buffer duration in seconds", OFFSET(buffer), AV_OPT_TYPE_INT64, { .i64 = 0 }, 0, INT64_MAX, FLAGS },
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
