FROM centos:7
WORKDIR /app/fuseupperfs

RUN yum install -y cmake make gcc fuse-devel fuse3-devel

COPY . .

RUN cmake -DCMAKE_BUILD_TYPE=Release . && \
  make

CMD [ "./fuseupperfs" ]