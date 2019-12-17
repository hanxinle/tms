======== Toy Media Server ========
Feature
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
