FROM simbricks/simbricks-base:jakob
RUN make -j `nproc` build-images-min COMPRESSED_IMAGES=true \
 && bash docker/cleanup_images.sh
