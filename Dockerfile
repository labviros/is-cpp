FROM ubuntu:16.04

# Change Accordingly
ENV GIT_URL http://192.168.1.101

COPY install /

RUN chmod +u+x ./install && ./install \
 && apt-get clean \
 && rm -rf /tmp/