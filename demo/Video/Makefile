all:
	arm-linux-gcc -o ./bin/camera ./src/*.c -I ./include -L ./lib -lapi_v4l2_arm -ljpeg -lpthread
clean:
	rm ./bin/camera
