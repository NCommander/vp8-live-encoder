#ffmpeg -video_size 1280x720 -f x11grab -i :0.0+100,200 -pix_fmt yuv420p -strict -1  -f yuv4mpegpipe - | sudo ./live_encoder  --rt --cpu-used=4 --end-usage=cbr --target-bitrate=27000 --fps=50/1 -v  --min-q=40 --max-q=56 --codec=vp8 --kf-max-dist=450 --lag-in-frames=0  --error-resilient=1  --threads=6 --token-parts=2 --timebase=1/1000 -o /tmp/test.webm -
stdbuf -o0 -i0 -e0 ffmpeg  -video_size 1280x720 -f x11grab -i :0.0+100,200 -pix_fmt yuv420p -strict -1  -f yuv4mpegpipe -r 30 - 2> /dev/null | stdbuf -o0 -i0 sudo ./live_encoder  --rt --cpu-used=4 --end-usage=cbr --target-bitrate=27000 --fps=30/1 -v  --min-q=40 --max-q=56 --codec=vp8 --kf-max-dist=5 --lag-in-frames=0  --error-resilient=1  --threads=6 --token-parts=2 --timebase=1/1000 --undershoot-pct=95 --token-parts=2 -q -o /tmp/test.webm -
