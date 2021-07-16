import subprocess
from datetime import datetime, timedelta
import time
import sys

loglevel = "error"
# stream_url = "https://showcase-content.cdn.dev.nativewaves.com/streams/f2d6b85363864e9dbb48687030372a6a/sessions/$latest/formats/hls/360p25/index-avcoder.m3u8"
# stream_url = "http://172.16.223.42:20405/1/streams/d55107fd9725499abf5f47aee4e88803/sessions/$latest/formats/hls/data/360p25/index-avcoder.m3u8"
stream_url = "http://172.16.222.234:20405/1/streams/6dbb8619d45e402fbcaa80fa05ec237d/sessions/$latest/formats/hls/data/360p25/index-avcoder.m3u8"
output = "decklink1"

if len(sys.argv)>1:
    output = sys.argv[1]

segment_length_s = 0.2
segment_length_ms = segment_length_s * 1000000
fps = 25
# stream_start = datetime(2021,7,14,16,3,23,0)


target_index = 0
target_time = datetime(2021,7,16,11,38,21,0)   # 1s more than timestamp in the playlist
# target_time = datetime(2021,7,14,16,3,35,0)



target_frame = target_index * segment_length_s * fps


print("target_index: " + str(target_index))
print("target_time: " + str(target_time))
print("target_frame: " + str(target_frame))
print("==========================")


new_target_time = datetime.now() + timedelta(seconds=5)
new_target_time = new_target_time - timedelta(microseconds=new_target_time.microsecond)



time_difference = new_target_time - target_time
time_difference_s = time_difference.total_seconds()
time_difference_ms = time_difference_s*1000000

segments_difference = time_difference_ms / segment_length_ms
start_index = int(target_index + segments_difference) - 3  # should be adjusted based on segment duration (e.g. should start 1 second before)
# start_index = int(target_index + segments_difference) + 10000  # should be adjusted based on segment duration (e.g. should start 1 second before)


print("time_difference_s: " + str(time_difference_s))
new_target_frame = int(time_difference_s*fps + target_frame)
start_frame = start_index * segment_length_s * fps
frames_to_discard = new_target_frame - start_frame

print("new_target_time: " + str(new_target_time))
print("new_target_frame: " + str(new_target_frame))

start_output = int(time.mktime(new_target_time.timetuple())*1000000)

print("start_index: " + str(start_index))
print("start_frame: " + str(start_frame))
print("frames_to_discard: " + str(frames_to_discard))


command = ["/home/ziemskib/Repo/ffmpeg-1936413/ffmpeg", "-loglevel", loglevel, "-f", "hls", "-live_start_index", str(start_index), "-copyts", "-i", stream_url, "-an", "-vf", "cue=buffer={}:cue={}".format(frames_to_discard, start_output), "-s", "1920x1080", "-pix_fmt", "uyvy422", "-f", "decklink", output]

print(command)

result = subprocess.run(command)
print("The exit code was: %d" % result.returncode)



