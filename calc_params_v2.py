import subprocess
from datetime import datetime, timedelta
import time
import sys

loglevel = "warning"
# stream_url = "https://showcase-content.cdn.dev.nativewaves.com/streams/f2d6b85363864e9dbb48687030372a6a/sessions/$latest/formats/hls/360p25/index-avcoder.m3u8"
# stream_url = "http://172.16.223.42:20405/1/streams/d55107fd9725499abf5f47aee4e88803/sessions/$latest/formats/hls/data/360p25/index-avcoder.m3u8"
stream_url = "http://172.16.222.234:20405/1/streams/6dbb8619d45e402fbcaa80fa05ec237d/sessions/$latest/formats/hls/data/360p25/index-avcoder.m3u8"
output = "decklink1"

if len(sys.argv)>1:
    output = sys.argv[1]

stream_started = datetime(2021,7,16,11,38,21,500000)
segment_length_s = 0.2


segment_length_ms = segment_length_s * 1000000

start_playback_time = datetime.now() + timedelta(seconds=5)
start_playback_time = start_playback_time - timedelta(microseconds=start_playback_time.microsecond)


time_difference = start_playback_time - stream_started
time_difference_s = time_difference.total_seconds()
time_difference_ms = time_difference_s*1000000

segments_difference = time_difference_ms / segment_length_ms
start_index = int(segments_difference) - 3  # should be adjusted based on segment duration (e.g. should start 1 second before)
# start_index = int(target_index + segments_difference) + 10000  # should be adjusted based on segment duration (e.g. should start 1 second before)

start_playback_time_ts = int(time.mktime(start_playback_time.timetuple())*1000000)


command = ["/home/ziemskib/Repo/ffmpeg-1936413/ffmpeg", "-loglevel", loglevel, "-f", "hls", "-live_start_index", str(start_index), "-copyts", "-i", stream_url, "-an", "-vf", "cue=buffer={}:cue={}".format(time_difference_ms, start_playback_time_ts), "-s", "1920x1080", "-pix_fmt", "uyvy422", "-preroll", "1", "-f", "decklink", output]

print(command)

result = subprocess.run(command)
print("The exit code was: %d" % result.returncode)



