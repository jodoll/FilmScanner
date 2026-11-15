# Project-scoped image for the film-scanner sandbox.
#
# Build film-scanner-toolchain:v1 first, then from the repository root:
#   docker build -f .sandbox/tools.Dockerfile -t film-scanner-tools:v1 .
#
# The project image reads the real platformio.ini and pre-installs the
# libraries into a PlatformIO libdeps directory in the image, so the clone
# starts with them instead of downloading them into .pio/libdeps. OpenCode 2
# and the toolchains come from the base image. Rebuild when platformio.ini
# changes.

FROM film-scanner-toolchain:v1

USER agent

# Dependency sources live in the image-backed core directory, not in the
# clone. .pio/build stays project-local.
ENV PLATFORMIO_LIBDEPS_DIR=/home/agent/.platformio/libdeps

# The copied file exists only for the install step and is removed with the
# scratch project below. The directory is created by the agent so the agent
# can delete it again.
RUN mkdir -p /tmp/warm

COPY --chown=agent:agent platformio.ini /tmp/warm/platformio.ini

# Install the declared lib_deps, then the test libraries the image is prepared
# for. --no-save keeps platformio.ini untouched. GoogleTest matches the native
# test runner's dependency (google/googletest@^1.17.0). Unity is not referenced
# by platformio.ini today; it is available for later embedded tests.
RUN cd /tmp/warm \
 && pio pkg install -e uno \
 && pio pkg install -e uno -l "throwtheswitch/Unity@^2.6.1" --no-save \
 && pio pkg install -e native -l "google/googletest@^1.17.0" --no-save \
 && rm -rf /tmp/warm

WORKDIR /home/agent/workspace

USER agent
