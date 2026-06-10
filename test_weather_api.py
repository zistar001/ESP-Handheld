#!/usr/bin/env python3
"""QWeather API 测试脚本 — 验证API密钥和接口是否可用"""
import urllib.request
import urllib.error
import json
import sys
import ssl
import gzip
import io

API_KEY = "700cf8ab08774bf089e52d33b89aecf8"
LOCATION = "101230501"

def test_api(url, label):
    print(f"\n=== {label} ===")
    print(f"URL: {url}")
    try:
        ctx = ssl._create_unverified_context()
        req = urllib.request.Request(url, headers={
            "User-Agent": "ESP32-S3-Test/1.0",
            "Accept-Encoding": "identity",  # 不要求压缩
        })
        resp = urllib.request.urlopen(req, timeout=15, context=ctx)
        raw = resp.read()
        print(f"HTTP {resp.status} ({len(raw)} bytes)")

        # 处理gzip压缩
        if raw[:2] == b'\x1f\x8b':
            raw = gzip.decompress(raw)
            print(f"  (gzip解压后 {len(raw)} bytes)")

        data = raw.decode("utf-8")
        obj = json.loads(data)
        print(f"Code: {obj.get('code')}")

        if obj.get("code") == "200":
            now = obj.get("now")
            if now:
                print(f"  天气: {now.get('text')}  {now.get('temp')}°C")
                print(f"  体感: {now.get('feelsLike')}°C  湿度: {now.get('humidity')}%")

            daily = obj.get("daily")
            if daily:
                for i, d in enumerate(daily):
                    print(f"  Day{i+1}: {d.get('fxDate')}  {d.get('textDay')}  "
                          f"{d.get('tempMin')}~{d.get('tempMax')}°C")
            return True
        else:
            print(f"  API错误: {json.dumps(obj, ensure_ascii=False)[:300]}")
            return False
    except urllib.error.HTTPError as e:
        print(f"  HTTP错误: {e.code}")
        body = e.read()
        if body[:2] == b'\x1f\x8b':
            body = gzip.decompress(body)
        print(f"  响应: {body.decode('utf-8', errors='ignore')[:300]}")
        return False
    except Exception as e:
        print(f"  异常: {type(e).__name__}: {e}")
        return False

if __name__ == "__main__":
    print(f"QWeather API 测试")
    print(f"API Key: {API_KEY[:8]}...{API_KEY[-4:]}")
    print(f"Location: {LOCATION}")

    test_api(
        f"https://devapi.qweather.com/v7/weather/now?location={LOCATION}&key={API_KEY}",
        "HTTPS devapi 当前天气"
    )
    test_api(
        f"https://devapi.qweather.com/v7/weather/3d?location={LOCATION}&key={API_KEY}",
        "HTTPS devapi 3天预报"
    )
    test_api(
        f"http://devapi.qweather.com/v7/weather/now?location={LOCATION}&key={API_KEY}",
        "HTTP devapi 当前天气"
    )

    print("\n=== 测试完成 ===")
