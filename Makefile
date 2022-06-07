CC = gcc
CFLAGS = -Wall -pedantic -Werror -Wextra -Wconversion -std=gnu11
TP6FLAGS = -lulfius -ljansson -lyder

all: create_logs compile setup_nginx setup_logrotate setup_systemd

create_logs:
	touch log_users_manager.log log_counter_manager.log

compile: counter_manager users_manager

counter_manager: counter_manager.c
	$(CC) $(CFLAGS) -o $@ $< $(TP6FLAGS)

users_manager: users_manager.c
	$(CC) $(CFLAGS) -o $@ $< $(TP6FLAGS)

setup_nginx:
	sudo cp tp6.conf /etc/nginx/conf.d/
	sudo systemctl restart nginx

setup_logrotate:
	sudo cp tp6_logrotate /etc/logrotate.d

setup_systemd:
	sudo cp users_manager.service /etc/systemd/system
	sudo cp counter_manager.service /etc/systemd/system
	sudo systemctl enable users_manager
	sudo systemctl enable counter_manager
	sudo systemctl start users_manager
	sudo systemctl start counter_manager
	sudo systemctl daemon-reload

clean:
	rm -f counter_manager users_manager *.log

uninstall:
	make clean
	sudo systemctl stop users_manager
	sudo systemctl stop counter_manager
	sudo systemctl disable users_manager
	sudo systemctl disable counter_manager
	sudo rm -rf /etc/systemd/system/users_manager.service
	sudo rm -rf /etc/systemd/system/counter_manager.service