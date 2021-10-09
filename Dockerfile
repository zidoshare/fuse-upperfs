FROM fedora:33
WORKDIR /app/fuseupperfs

RUN yum install -y cmake make gcc fuse-devel leveldb-devel

COPY . .

RUN cmake -DCMAKE_BUILD_TYPE=Release . && \
  make

CMD [ "./fuseupperfs" ]