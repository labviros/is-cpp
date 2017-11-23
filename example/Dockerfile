FROM picoreti/is-cpp:1
ARG SERVICE=local
RUN git clone http://192.168.1.101/labviros/${SERVICE}
RUN cd ${SERVICE} && \
    make service && \
    mkdir lib && \
    for lib in `ldd service | awk 'BEGIN{ORS=" "}$1~/^\//{print $1}$3~/^\//{print $3}' | sed 's/,$/\n/'`; do cp $lib lib/; done 

FROM ubuntu:16.04
ARG SERVICE=local
COPY --from=0 ${SERVICE}/service .
COPY --from=0 ${SERVICE}/lib /usr/local/lib/
RUN ldconfig
CMD ["./service"]