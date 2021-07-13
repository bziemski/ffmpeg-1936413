import subprocess
from datetime import datetime, timedelta
import time

loglevel = "warning"
stream_url = "https://showcase-content.cdn.dev.nativewaves.com/streams/f2d6b85363864e9dbb48687030372a6a/sessions/$latest/formats/hls/1080p25/index.m3u8"
output = "decklink1"

segment_length_s = 3
segment_length_ms = segment_length_s * 1000000
fps = 25



target_index = 4112
target_time = datetime(2021,7,13,15,30,0,0)
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
start_index = int(target_index + segments_difference) - 0  # should be adjusted based on segment duration (e.g. should start 1 second before)


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

command = ["/home/ziemskib/Repo/ffmpeg-1936413/ffmpeg", "-loglevel", loglevel, "-f", "hls", "-live_start_index", str(start_index), "-i", stream_url, "-an", "-vf", "cue=buffer={}:cue={}".format(frames_to_discard, start_output), "-pix_fmt", "uyvy422", "-f", "decklink", output]

print(command)

result = subprocess.run(command)
print("The exit code was: %d" % result.returncode)



