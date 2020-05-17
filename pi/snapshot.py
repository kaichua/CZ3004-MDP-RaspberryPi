from picamera import PiCamera
import time
import sys
import os.path

save_path = "/home/pi/Desktop/pi_photos/"

def main(argv):
    try:
        filename = sys.argv[1]            
        camera = PiCamera()
        camera.resolution = (450, 400)
        camera.hflip = True
        complete = os.path.join(save_path, str(filename)+".jpg")
        #camera.shutter_speed = 1000000/60
        camera.awb_mode = 'tungsten'
        camera.capture(complete)
        print("done")
    except:
        print("fail")  


if __name__ == "__main__":
    main(sys.argv[1:])
