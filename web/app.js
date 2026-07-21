(function () {
    var fpsEl = document.getElementById("fps");
    var uptimeEl = document.getElementById("uptime");
    var recordingEl = document.getElementById("recording");
    var streamingEl = document.getElementById("streaming");
    var streamEl = document.getElementById("stream");
    var msgEl = document.getElementById("msg");
    var btnRecStart = document.getElementById("btnRecStart");
    var btnRecStop = document.getElementById("btnRecStop");
    var btnStreamStart = document.getElementById("btnStreamStart");
    var btnStreamStop = document.getElementById("btnStreamStop");
    var rtspUrlEl = document.getElementById("rtspUrl");

    rtspUrlEl.textContent = "rtsp://" + location.hostname + ":8554/live";

    function formatUptime(sec) {
        var h = Math.floor(sec / 3600);
        var m = Math.floor((sec % 3600) / 60);
        var s = sec % 60;
        if (h > 0) {
            return h + "h " + m + "m " + s + "s";
        }
        if (m > 0) {
            return m + "m " + s + "s";
        }
        return s + "s";
    }

    function setMsg(text) {
        msgEl.textContent = text || "";
    }

    function refreshStatus() {
        fetch("/status")
            .then(function (res) { return res.json(); })
            .then(function (data) {
                fpsEl.textContent = data.fps;
                fpsEl.className = data.fps > 0 ? "ok" : "err";
                uptimeEl.textContent = formatUptime(data.uptime);
                recordingEl.textContent = data.recording ? "录像中" : "未录像";
                recordingEl.className = data.recording ? "warn" : "";
                streamingEl.textContent = data.streaming ? "推流中" : "未推流";
                streamingEl.className = data.streaming ? "ok" : "";
                streamEl.textContent = data.stream_lost ? "断流" : "正常";
                streamEl.className = data.stream_lost ? "err" : "ok";
            })
            .catch(function () {
                fpsEl.textContent = "--";
                streamEl.textContent = "连接失败";
                streamEl.className = "err";
            });
    }

    function postAction(path, okMsg) {
        fetch(path, { method: "POST" })
            .then(function (res) { return res.json(); })
            .then(function (data) {
                if (data.error) {
                    setMsg(data.error);
                } else {
                    setMsg(okMsg);
                }
                refreshStatus();
            })
            .catch(function () {
                setMsg("请求失败");
            });
    }

    btnRecStart.addEventListener("click", function () {
        postAction("/record/start", "已开始录像");
    });

    btnRecStop.addEventListener("click", function () {
        postAction("/record/stop", "已停止录像");
    });

    btnStreamStart.addEventListener("click", function () {
        postAction("/stream/start", "已开始 RTSP 推流");
    });

    btnStreamStop.addEventListener("click", function () {
        postAction("/stream/stop", "已停止 RTSP 推流");
    });

    refreshStatus();
    setInterval(refreshStatus, 1000);
})();
