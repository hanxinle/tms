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
- [] MCU
- [] media server forward

* Support Input: rtmp, rtp(webrtc)
* Support Output: rtmp/http-flv/http-hls/https-flv/https-hls/rtp(webrtc)

## Usage

**Step 1:** get source code
```
git clone https://github.com/HuyaJohn/trs.git
cd trs
```

**Step 2:** fetch depend
```
cd depend
sh depend.sh
```

**Step 3:** build
```
cd ../src
make
```
