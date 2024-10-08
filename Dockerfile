FROM ubuntu:latest

# Install necessary packages
RUN apt-get update && apt-get install -y \
    bash \
    coreutils \
    socat \
    && rm -rf /var/lib/apt/lists/*

# Set your working directory
WORKDIR /app

# Copy your script and any necessary files
COPY tools/ tools/
COPY demo/ demo/

# Run your script as an entry point
CMD ["bash", "tools/gentree.sh", "tools/demo.tree"]
