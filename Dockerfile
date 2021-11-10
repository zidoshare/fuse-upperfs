FROM fedora:33
WORKDIR /app/fuseupperfs

RUN yum install -y cmake make gcc fuse3-devel leveldb-devel

COPY . .

RUN cmake -DCMAKE_BUILD_TYPE=Release -DFUSE3=1 . && \
  make

CMD [ "./fuseupperfs" ]
