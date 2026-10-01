package com.pocketmodeler.app

import java.io.IOException
import java.util.concurrent.TimeUnit
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response

/**
 * C++ 出站 HTTP 全部经此实现（宪法铁律 4：C++ 不碰 SSL，Kotlin OkHttp 负责 TLS）。
 * JNI 桥把 {method,url,headers,body,timeout_ms,max_bytes} 换成 byte[] 往返：
 * request 打包为 [headers_len, url, body] → response 解包回 [status, headers, body]。
 * 打包格式见 jni_bridge.cpp 的 http_transport_impl，两侧严格对称。
 */
class HttpTransportImpl(private val client: OkHttpClient = defaultClient()) {

    fun send(
        method: String,
        url: String,
        headers: Array<String>,    // 扁平 [k1,v1,k2,v2,...]
        body: ByteArray?,
        timeoutMs: Long,
        maxBytes: Long
    ): OkHttpResult {
        if (url.length > 8 * 1024) {
            return OkHttpResult(ok = false, status = 0, error = "url_too_long")
        }
        val builder = Request.Builder().url(url)

        var i = 0
        while (i + 1 < headers.size) {
            builder.header(headers[i], headers[i + 1])
            i += 2
        }
        if (body != null) {
            builder.method(method.uppercase(), body.toRequestBody(null))
        } else {
            builder.method(method.uppercase(), null)
        }

        // 用请求级超时避免全局 client 被单个慢请求污染。
        val scoped = client.newBuilder()
            .callTimeout(timeoutMs, TimeUnit.MILLISECONDS)
            .build()

        try {
            scoped.newCall(builder.build()).execute().use { resp ->
                val status = resp.code
                if (resp.body == null) {
                    return OkHttpResult(ok = status in 200..299, status = status, body = ByteArray(0))
                }
                val length = resp.body!!.contentLength()
                if (length > maxBytes) {
                    return OkHttpResult(ok = false, status = status, error = "response_too_large")
                }
                val bytes = resp.body!!.bytes()
                if (bytes.size.toLong() > maxBytes) {
                    return OkHttpResult(ok = false, status = status, error = "response_too_large")
                }
                return OkHttpResult(ok = status in 200..299, status = status, body = bytes)
            }
        } catch (e: IOException) {
            return OkHttpResult(ok = false, status = 0, error = e.message ?: "io_error")
        } catch (e: IllegalArgumentException) {
            return OkHttpResult(ok = false, status = 0, error = e.message ?: "bad_url")
        }
    }

    /** 扁平化响应头（重复头合并为逗号分隔，够 C++ 侧读 Content-Length/Content-Type 即可）。 */
    fun flattenHeaders(resp: Response): Array<String> {
        val out = mutableListOf<String>()
        resp.headers.forEach { (k, v) -> out += k; out += v }
        return out.toTypedArray()
    }

    private companion object {
        fun defaultClient(): OkHttpClient =
            OkHttpClient.Builder()
                .connectTimeout(15, TimeUnit.SECONDS)
                .readTimeout(15, TimeUnit.SECONDS)
                .writeTimeout(15, TimeUnit.SECONDS)
                .build()
    }
}

/** 与 C++ side 的 HttpResponse 对称的扁平结果。 */
data class OkHttpResult(
    val ok: Boolean,
    val status: Int = 0,
    val headers: Array<String> = emptyArray(),
    val body: ByteArray = ByteArray(0),
    val error: String = ""
)