#	round-robin eth

##	Description

round-robin eth is a virtual linux ethernet device driver which implements
a round-robin balancing alogrithm distributing in&out packets between the 
lower devices. 


##	Dependencies

In order to run, this software require the following dependencies:
	
	Linux Kernel 3.8


##	Implementation Details

Balance network traffic between devices, I add a virtual layer(the r-r eth)
between the upper applications and lower devices.

###	intercept incomes

Register a rx handler upon the the lower device, redirect the income packet 
to r-r eth.


###	distribute outcomes

Configure r-r eth to be the default gateway route path.


## Usages

###	display route table
	
	route -n

### up/down dev
	
	ifconfig ethx up
	ifconfig ethx down

### configure dev

	//assign r-r eth the ip 192.168.1.xxx 
	ifconfig ethx 192.168.1.xxx netmask 255.255.255.0

###	add&del dev routes

	//del device's route path
	route del -net 192.168.1.0/24 dev eth[0-*] 

	//add a new route path, target network segment is 192.168.1.0/24,
	//through device eth[0-*], metric 1.
	route add -net 192.168.1.0/24 dev eth[0-*] metric 1 


###	add&del gateway routes

	//del the default gateway route 
	route del default 

	// config r-r eth to be the gateway route device
	route add default gw 192.168.1.1 dev ethx 
	







