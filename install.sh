#!/bin/bash -e
printf "checking dependencies..."

if ! [ -x "$(command -v systemd)" ]
then
printf "\n(required) installing systemd...\n"
apt update
sudo apt install systemd -y
printf "\n(required) systemd successfully installed"
fi

if ! [ -x "$(command -v nginx)" ]
then
printf "\n(required) installing nginx..."
apt update
sudo apt install nginx -y
sudo systemctl reload nginx.service
printf "\n(required) nginx successfully installed"
fi

if ! [ -x "$(command -v logrotate)" ]
then
printf "\n(required) installing logrotate..."
apt update
sudo apt install logrotate -y
printf "\n(required) logrotate successfully installed"
fi

printf "\ndependencies check done"
printf "\ncompiling project...\n"
make clean && make
printf "project compiled successfully\n"