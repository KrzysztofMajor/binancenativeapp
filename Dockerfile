FROM darthulyk/dockervcpkg:latest

RUN git clone https://github.com/KrzysztofMajor/binancenativeapp.git /opt/binancenativeapp
WORKDIR /opt/binancenativeapp

RUN cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build -- -j3

FROM ubuntu:20.10
WORKDIR /root
COPY --from=0 /opt/binancenativeapp/cacert.pem .
COPY --from=0 /opt/binancenativeapp/build/binancenativeapp .

ENTRYPOINT ["./binancenativeapp", "--cert=cacert.pem"]
CMD ["--symbols=BTCUSDT;ETHUSDT;DOGEUSDT"]
