FROM centos:7
WORKDIR /app/fusequota

RUN yum install -y autoconf automake make gcc fuse-devel

COPY . .

RUN autoreconf --install && \
    ./configure && \
    make && \
    make install && \
    rm -rf /app/fusequota

CMD [ "fusequota" ]