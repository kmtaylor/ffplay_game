game_audio=battle.mp3
game_image="battle sample.jpg"

mpeg_opts="mpeg2video -mbd rd -trellis 2 -cmp 2 -subcmp 2 -g 100"
still_opts="-shortest -vf scale=640:360"

#ffmpeg -loop 1 -i "$game_image" -i "$game_audio" $still_opts game.mp4
ffmpeg -framerate 1 -pattern_type glob -i 'battle-*.jpg' \
	       -i $game_audio -r 25 -vf scale=640:360 game.mp4

#exit
attract_mode=hulkfists.mp4	    # Stream 0
countdown=countdown.mp4		    # Stream 1
game_screen=game.mp4		    # Stream 2
winner1=hbwins.mp4		    # Stream 3
winner2=hulkwins.mp4		    # Stream 4

ffmpeg -i $attract_mode -i $countdown -i $game_screen -i $winner1 -i $winner2 \
	       -map 0 -map 1 -map 2 -map 3 -map 4 -c:v $mpeg_opts media.mpg
