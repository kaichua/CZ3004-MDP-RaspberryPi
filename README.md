# CZ3004-MDP-RaspberryPi

## Introduction
Multi Disciplinary Project (MDP) is part of the CZ3004 coursework offered by Nanyang Technological University. <br/>This repository only covers the Raspberry Pi portion of the project which I was in charge of the RasberryPi's communications written in C. <br/>The rest of the portion would cover some tips and my experience from this project which I hope will help you.

## Notes
<b>Always backup your sd card when you finalize changes so that they can be reverted back when needed!!</b><br/>At least once after you have performed the installation and followed the guide provided by NTU.

<b>Connecting to the Pi</b><br/>For the initial setup, if you do not own a mouse, monitor and keyboard, head over to the labs and use the ones that are connected to the desktops instead. **Remember to plug back the peripherals once you are done!** It is possible to use virtual machines to setup as well but I would not recommend for beginners.<br/>

For beginners who are not used to using CLI to perform configurations via the PuTTY outside of labs, make sure you enable VNC on your Pi and perform a remote desktop connection to work with Pi's GUI.<br/> 

Outside of labs, you are also able to use a RJ45 LAN cable to connect your Pi to the router and remotely connect to it as you did in the labs. To avoid the dynamic changing of your Pi's IP address outside of labs, you can reserve an IP address for your Pi based on its MAC address under your router's main configuration page. 

<b>File Permissions</b><br/>If you encounter permission errors when modifying files, the simplest method is to execute `sudo chmod 777 <filename>` but this is unsafe in actual practice.

<b>Data Transmission Errors</b><br/>You might encounter the following problems when dealing with data sent from a device to another:<br/> 
 1. Inconsistent data received
		- Sender sent "Completed" but receiver received inconsistent data such as "Compl" / "Complet" / "Com" / etc..
 2. Expected multiple outcomes but only received one
		 - Sender sent out "Completed" and "Done" in the following order but the receiver only receives "Completed"

This might be due to two issues I have experienced:<br/> 
1. Baud rate difference between the Arduino and RPI.
2. Lack of `\n` which acts as the enter key when sending data through TCP/IP connection.

## Pre-requisites
NTU provides a comprehensive set of guide to configure the basic functionalities required for the Raspberry Pi but it is best to have some knowledge about the following before you start on the project:

- **Raspian Environment**
	- Raspian is the operating system that is used for the Raspberry Pi. There are many version but I recommend using the version provided by the school so that you can seek help easier from the seniors and professors.

	- To Edit Files
		- There are many ways to edit files in the Debian-based environment:
			- Graphical:
				- Find the file > Double click on it to edit on the GUI
				- `sudo gedit <directory name>`
			- Command Line Interface:
				- `sudo nano <directory name>`
				- `sudo vi <directory name>`
	- When in doubt, always execute `sudo man <command>` to view the manual available for that command. E.g. `man man` gives you the manual for manual!
	- ***NOT RECOMMENDED***, but to remove the need for sudo, you can run `sudo su` but this does not protect you from accidentally modifying important files.
	- The default username is "pi" and password is "raspberry".
	-  Use the Tab & Tab Tab key to auto complete & view directory paths.
	- Other important commands include `ls, cd, pwd, grep, cat, echo and many more....`

- **Networking Concepts**

	- **IP Addresses**
		- To provide a form of identification to each host that are connected to the network.
			- E.g. An IP address of 192.168.1.1/24, in this case 192.168.1.x represents the network and x represents an integer from \[1:254]  given to individual hosts. This set of IP address belongs to the private domain which cannot be used on the internet.<br/>


  - **DHCP (Dynamic Host Configuration Protocol)**
    - To provide IP addresses to each host that are connected to the network.
      - In the IP address example, the DHCP server can assign any integer x from [1:254] as hosts get connected to the network.<br/>

  - **SSH**
    - To provide a secure tunnel to establish a connection to the Pi.
      - This is mainly used for file transfers and system configurations.
    - Tools that I used to upload / modify files on the Pi:
      - Visual Studio Code
        - Using the SSH plugin available, this method is the most effortless when it comes to modifying your files on the Pi.
      - WinSCP
        - For uploading files and directory lookup in a graphical format.

  - **Python OR C**
    - Python would be the preferred choice to be used as compared to C from what I noticed as it is easier to understand and code.
    - Python has more libraries that can be found and used as compared to C.
      - One problem I had was the lack of support for picam.
    - The only reason to choose C over Python is for the multi-threading that C can offer while Python does not.
      - From my research, Python's version of multi-threading is limited by Global Interpreter Lock (GIL) which prevents another thread from accessing objects when one thread is currently doing so which incurs unnecessary overheads.
        - Multi-processing is another option which you can look into to speed up your Python code.

## Inner Working
The pi folder contains the following:
 - compile.txt
	 - Used to compile the C program by executing the command in it.
 - main.c
	 - Main program for the RPI to act as a communication hub.
 - settings.h
	 - To store and access system configuration options.
 - snapshot.py
	 - To take photographs using the picam module.
 
I had used the RPI solely as a communication hub. In order to identify the images taken by picam, I created a folder within RPI to store the photos taken where it would be synchronized onto the PC running the CNN model. This PC would retrieve the image, delete the image from the folder and attach the processed image into another folder in the RPI. One of the threads I had implemented was to check for any updates to the folder hosting the processed image and by using the name of the photo, the RPI is able to update the other devices on the detected values.
