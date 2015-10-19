#!/bin/bash
ffmpeg -video_size 1024x768 -f x11grab -i :0.0 -pix_fmt yuv420p -strict -1  -f yuv4mpegpipe - | ./live_encoder --buf-initial-sz=500 --buf-optimal-sz=600 --buf-sz=1000 --cpu-used=-6 --end-usage=cbr --error-resilient=1 --kf-max-dist=5 --lag-in-frames=0 --max-intra-rate=300 --max-q=63 --min-q=2 --noise-sensitivity=0 --rt --yv12 --codec=vp8 --webm --timebase=1001/30000 -o /tmp/stream -
