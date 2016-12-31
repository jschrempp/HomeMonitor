# HomeMonitor
This project is based on code for the "Standalone Intelligent Sensor" system by Team Practical Projects. The TPP/SISProject repository is listed as the upstream master. The work on this project will all be done on the HMMaster branch to keep it separate from the SISProject. Hopefully this is the correct way to set up a git fork of a fork - I'm uncertain if this has messed up the history, I hope not.

This project is a monitoring system for a physical space. It uses inexpensive off the shelf wireless sensors. The code will send alerts through IFTTT to your personal smart phone. There is a javascript client that is used to configure, control, and view history.

We are always looking for people who are interested in working on this. Contact me.

As of December 2016 I'm just starting this. Some things on my to-do list:

* Break the firmware into several files. The wireless communication, the inferences, the alerting. At least these three.
* Do away with global arrays for the ISR communication by moving to structures that are passed around.
* Extend the types of sensors and events that are handled.
* Optimize the communication between the control client and the monitor

Send us a message to join in the fun: SISProject@shrimpware.com

Read about my other work at [Shrimpware](http://www.shrimpware.com)

