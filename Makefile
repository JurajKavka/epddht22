ip = 192.168.200.50
PIPENV = pipenv run

all:  mini-debug 

mini-debug:
	$(PIPENV) platformio run --environment pro8MHzatmega328-debug 

mini-release:
	$(PIPENV) platformio run --environment pro8MHzatmega328-release

nano-debug:
	platformio run --environment nano-debug 

release:
	platformio run --environment uno-release

clean:
	platformio -f -c vim run --target clean

program:
	platformio -f -c vim run --target program

uploadfs:
	platformio -f -c vim run --target uploadfs

update:
	platformio -f -c vim update

init:
	pio init -b uno --ide vim 

monit:
	pio device monitor -b 115200

ping:
	ping $(ip)

