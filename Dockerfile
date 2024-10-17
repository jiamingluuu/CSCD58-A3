FROM ubuntu:22.04

RUN apt-get update \
&& DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    bash \
    build-essential \
    gdb \
    gcc \
    vim \
    python3 \
    python3-pip \
    sudo \
    iproute2 \
    openvswitch-switch \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash q && \
    echo "q:cscd58router" | chpasswd

RUN usermod -aG sudo q

RUN echo 'q ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER q

RUN python3 -m pip install setuptools

RUN mkdir -p /home/q/lab3/
COPY . /home/q/lab3/
WORKDIR /home/q/lab3/
RUN cd /home/q/lab3/
CMD ["lab3"]
