
FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    gdb \
    git \
    python3-matplotlib \
    python3-pandas \
    nlohmann-json3-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt

RUN git clone --depth 1 --branch foxglove-202510 https://github.com/ogdf/ogdf.git \
    && cmake -S /opt/ogdf -B /opt/ogdf/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DOGDF_WARNING_ERRORS=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build /opt/ogdf/build --parallel $(nproc) \
    && cmake --install /opt/ogdf/build \
    && rm -rf /opt/ogdf/.git

WORKDIR /workspaces/Seminararbeit/Graph
