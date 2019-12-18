## tms

tms(toy media server) is a toy media server for myself learning media develop.

## Feature
- [x] publish media by rtmp
- [x] publish media by webrtc
- [x] plya media by rtmp
- [x] play media by http-flv
- [x] play media by http-hls
- [x] play media by webrtc
- [x] media demux/remux, like rtmp(flv) to webrtc(rtp)
- [ ] transcode
- [ ] MCU
- [ ] media server forward, current tms is only a single server node

## Usage

**Step 1:** get source code
```
git clone https://github.com/HuyaJohn/tms.git
cd tms
```

**Step 2:** fetch depend
```
cd depend
./depend.sh
```

**Step 3:** build
```
cd ../src
make
```
**Step 4:** run
```
./tms -server_ip xxx.xxx.xxx.xxx
```

## Example
### publish rtmp using obs, play flv/hls/rtmp using vlc
