import cv2
import numpy as np
import os
import glob
import shutil
import time
import math

#if an image have 2 ids, file name is: 13_13_8_19.jpg
#pi image size 450 x 400

# Load Yolo

print('Loading yolo config...')
net = cv2.dnn.readNet("rpi_best.weights", "rpi.cfg")
classes = []
with open("rpi.names", "r") as f:
    classes = [line.strip() for line in f.readlines()]
layer_names = net.getLayerNames()
output_layers = [layer_names[i[0] - 1] for i in net.getUnconnectedOutLayers()]
colors = np.random.uniform(0, 255, size=(len(classes), 3))

path = 'C:\\Users\\xingyu\\Desktop\\image\\syncimage\\'
resulthave = "C:\\Users\\xingyu\\Desktop\\image\\have"
resultnohave= "C:\\Users\\xingyu\\Desktop\\image\\nohave"
final_image_path = "C:\\Users\\xingyu\\Desktop\\image\\final_image\\"
waitcount = 0
print('Done loading config!')

while True:
  try:
    files = []
    # r=root, d=directories, f = files
    for r, d, f in os.walk(path):
        for file in f:
            if '.jpg' in file:
                files.append(os.path.join(r, file))
                files.append(file)
    if len(files):
      for f in files:
          #file name
          #print('processing..'+f)
        if os.path.isfile(f) and os.access(f, os.R_OK):
    
    
          # Loading image
          img = cv2.imread(f, cv2.IMREAD_COLOR)

          #may produce error
          #img = cv2.resize(img, None, fx=0.4, fy=0.4)
          #handle not valid image
        

          try:
            height, width, channels = img.shape
          except:
            #remove file
            os.remove(f)
            print('img not stable')

          #check size 
          # Detecting objects
          blob = cv2.dnn.blobFromImage(img, 0.00392, (416, 416), (0, 0, 0), True, crop=False)
          net.setInput(blob)
          outs = net.forward(output_layers)

          # Showing informations on the screen
          confidencelvl = 0.7
          class_ids = []
          confidences = []
          boxes = []
          for out in outs:
              for detection in out:
                  scores = detection[5:]
                  class_id = np.argmax(scores)
                  confidence = scores[class_id]
                  if confidence > confidencelvl:
                      # Object detected
                      center_x = int(detection[0] * width)
                      center_y = int(detection[1] * height)
                      w = int(detection[2] * width)
                      h = int(detection[3] * height)

                      # Rectangle coordinates
                      x = int(center_x - w / 2)
                      y = int(center_y - h / 2)

                      boxes.append([x, y, w, h])
                      confidences.append(float(confidence))
                      print(float(confidence))
                      class_ids.append(class_id)

          indexes = cv2.dnn.NMSBoxes(boxes, confidences, confidencelvl, 0.4)

          label=''
          region=''

          fname1=f.split('\\')

          #this is a file name
          name1 = fname1[-1]

          #for region
          name2 = name1.split('%')



          font = cv2.FONT_HERSHEY_PLAIN
          for i in range(len(boxes)):
              if i in indexes:
                  x, y, w, h = boxes[i]

                  line1=0
                  line2 = math.floor(width/3) 
                  line3 = math.floor(line2 * 2)
                  line4 = math.floor(width)

                  x1 = x
                  x2 = x+w

                  #w should'nt be <100,ignore bouding box less than half of size
                  if w > 240:
                    label = str(classes[class_ids[i]])
                    color = colors[i]
                    cv2.rectangle(img, (x, y), (x + w, y + h), color, 10)
                    cv2.putText(img, label, (x+10, y + 60), font, 5, color, 3)

                    if x2 < line2:
                      print(label+' at region 1')
                      name_region1 = label+'_'+name2[0]+'.jpg'
                      #check if image already in the final_image folder & skip it if exists
                      #im[y1:y2, x1:x2]

                      if not os.path.isfile(final_image_path+name_region1):
                          crop1 = img[10:height, 0:line2]
                          cv2.imwrite('C:\\Users\\xingyu\\Desktop\\image\\final_image\\'+name_region1, crop1)
                      else:
                          print(name_region1 +' already exists')
                      #move to have folder

                    if x1 > line2-100 and x2 < line3+100:
                      print(label+' at region 2')
                      region = 'r2'
                      name_region2 = label+'_'+name2[1]+'.jpg'
                      
                      if not os.path.isfile(final_image_path+name_region2):
                          crop2 = img[10:height, line2:line2+line2]
                          cv2.imwrite('C:\\Users\\xingyu\\Desktop\\image\\final_image\\'+name_region2, crop2)
                      else:
                          print(name_region2 +' already exists')
                      #move to have folder

                    if x1 > line3:
                       print(label+' at region 3')
                       region = 'r3'
                       name_region3 = label+'_'+name2[2]
                       if not os.path.isfile(final_image_path+name_region3):
                           crop3 = img[10:height, line3:width]
                           cv2.imwrite('C:\\Users\\xingyu\\Desktop\\image\\final_image\\'+name_region3, crop3)
                       else:
                           print(name_region3 +' already exists')
                             
          #valid image
          if label != '':
              print(name1+' id: '+label)
              shutil.move(os.path.join(path, str(name1)), os.path.join(resulthave, str(name1)))

          else:
            print(f+'--> no result')
            #move to nohave folder
            shutil.move(os.path.join(path, str(name1)), os.path.join(resultnohave, str(name1)))
        else:
          #ignore for no result images

          try:
            print(name1+'--> file not ready, try again later')

          except:
            print('name1 not found')

    else:
      waitcount+=1
      print('waiting for incoming files..'+str(waitcount))

  except:
     print('error,skip')
  time.sleep(2)
