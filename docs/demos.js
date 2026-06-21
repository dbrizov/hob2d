// Live demos for the Hob2D docs site.
// These recreate engine behaviour in the browser so the docs need no captured gifs:
//   - idle sprite: cycles the real PlayerIdle frames, like the engine's SpriteAnimator
//   - healthbar:   animates the fill via a CSS width transition, like Player:set_health

(function () {
    "use strict";

    // --- Sprite animation (cycles frames like the engine's SpriteAnimator) ---
    function playClip(elementId, frames, fps) {
        var sprite = document.getElementById(elementId);
        if (!sprite) return;

        var index = 0;
        setInterval(function () {
            index = (index + 1) % frames.length;
            sprite.src = frames[index];
        }, 1000 / fps);
    }

    // --- Psychedelic shader (WebGL port of assets/shaders/psychedelic.frag.hlsl) ---
    // Applies the real shader's three effects to the idle sprite texture:
    // wavy UV warp, radial chromatic aberration, and scrolling per-column hue rotation.
    function startShaderDemo() {
        var canvas = document.getElementById("shader-sprite");
        if (!canvas) return;

        var gl = canvas.getContext("webgl") || canvas.getContext("experimental-webgl");
        if (!gl) {
            canvas.style.display = "none";
            return;
        }

        var vertSrc =
            "attribute vec2 a_pos;" +
            "varying vec2 v_uv;" +
            "void main() {" +
            "  v_uv = vec2(a_pos.x * 0.5 + 0.5, 0.5 - a_pos.y * 0.5);" +
            "  gl_Position = vec4(a_pos, 0.0, 1.0);" +
            "}";

        var fragSrc =
            "precision mediump float;" +
            "uniform sampler2D u_tex;" +
            "uniform vec2 u_texel;" +
            "uniform float u_time;" +
            "varying vec2 v_uv;" +
            "vec3 hue_shift(vec3 rgb, float angle) {" +
            "  vec3 k = vec3(0.57735);" +
            "  float ca = cos(angle); float sa = sin(angle);" +
            "  return rgb * ca + cross(k, rgb) * sa + k * dot(k, rgb) * (1.0 - ca);" +
            "}" +
            "void main() {" +
            "  vec2 uv = v_uv;" +
            "  vec2 warp = vec2(" +
            "    sin(uv.y * 25.0 + u_time * 4.0) * u_texel.x * 3.0," +
            "    sin(uv.x * 20.0 + u_time * 3.0) * u_texel.y * 3.0);" +
            "  vec2 wuv = uv + warp;" +
            "  float center_a = texture2D(u_tex, wuv).a;" +
            "  if (center_a <= 0.1) discard;" +
            "  vec2 from_center = wuv - 0.5;" +
            "  float ab_strength = 0.06 + 0.04 * sin(u_time * 2.0);" +
            "  vec2 ab = from_center * length(from_center) * ab_strength;" +
            "  float r = texture2D(u_tex, wuv + ab).r;" +
            "  float g = texture2D(u_tex, wuv).g;" +
            "  float b = texture2D(u_tex, wuv - ab).b;" +
            "  vec3 rgb = vec3(r, g, b);" +
            "  rgb = hue_shift(rgb, wuv.x * 6.28318 + u_time * 2.0);" +
            "  gl_FragColor = vec4(rgb, center_a);" +
            "}";

        function compile(type, src) {
            var s = gl.createShader(type);
            gl.shaderSource(s, src);
            gl.compileShader(s);
            return s;
        }

        var program = gl.createProgram();
        gl.attachShader(program, compile(gl.VERTEX_SHADER, vertSrc));
        gl.attachShader(program, compile(gl.FRAGMENT_SHADER, fragSrc));
        gl.linkProgram(program);
        gl.useProgram(program);

        var buffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
        gl.bufferData(gl.ARRAY_BUFFER,
            new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
        var aPos = gl.getAttribLocation(program, "a_pos");
        gl.enableVertexAttribArray(aPos);
        gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);

        gl.enable(gl.BLEND);
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

        var uTexel = gl.getUniformLocation(program, "u_texel");
        var uTime = gl.getUniformLocation(program, "u_time");

        var paths = [
            "media/player/HJ_idle01.png",
            "media/player/HJ_idle02.png",
            "media/player/HJ_idle03.png",
            "media/player/HJ_idle04.png",
        ];

        var textures = [];
        var size = null;
        var loaded = 0;

        paths.forEach(function (path, i) {
            var image = new Image();
            image.onload = function () {
                var tex = gl.createTexture();
                gl.bindTexture(gl.TEXTURE_2D, tex);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
                textures[i] = tex;
                if (!size) size = { w: image.width, h: image.height };

                if (++loaded === paths.length) {
                    canvas.width = size.w;
                    canvas.height = size.h;
                    gl.viewport(0, 0, canvas.width, canvas.height);
                    gl.uniform2f(uTexel, 1.0 / size.w, 1.0 / size.h);

                    var index = 0;
                    var fps = 4;
                    var frameStart = performance.now();
                    var start = performance.now();
                    (function frame() {
                        var now = performance.now();
                        if (now - frameStart >= 1000 / fps) {
                            index = (index + 1) % textures.length;
                            frameStart = now;
                        }
                        gl.bindTexture(gl.TEXTURE_2D, textures[index]);
                        gl.uniform1f(uTime, (now - start) / 1000);
                        gl.clearColor(0, 0, 0, 0);
                        gl.clear(gl.COLOR_BUFFER_BIT);
                        gl.drawArrays(gl.TRIANGLES, 0, 6);
                        requestAnimationFrame(frame);
                    })();
                }
            };
            image.src = path;
        });
    }

    function startSpriteDemos() {
        // PlayerIdle clip: 4 frames @ 4 fps, looping
        playClip("idle-sprite", [
            "media/player/HJ_idle01.png",
            "media/player/HJ_idle02.png",
            "media/player/HJ_idle03.png",
            "media/player/HJ_idle04.png",
        ], 4);
    }

    // --- Run clip with the engine's outline shader (assets/builtin/shaders/sprite.frag.hlsl) ---
    // Cycles the 10 PlayerRun frames @ 15 fps and paints a red silhouette outline:
    // transparent texels with any opaque neighbor are filled with the outline color.
    function startRunOutlineDemo() {
        var canvas = document.getElementById("run-sprite");
        if (!canvas) return;

        var gl = canvas.getContext("webgl") || canvas.getContext("experimental-webgl");
        if (!gl) {
            canvas.style.display = "none";
            return;
        }

        var vertSrc =
            "attribute vec2 a_pos;" +
            "varying vec2 v_uv;" +
            "void main() {" +
            "  v_uv = vec2(a_pos.x * 0.5 + 0.5, 0.5 - a_pos.y * 0.5);" +
            "  gl_Position = vec4(a_pos, 0.0, 1.0);" +
            "}";

        var fragSrc =
            "precision mediump float;" +
            "uniform sampler2D u_tex;" +
            "uniform vec2 u_texel;" +
            "uniform vec4 u_outline_color;" +
            "uniform float u_outline_width;" +
            "varying vec2 v_uv;" +
            "void main() {" +
            "  vec4 center = texture2D(u_tex, v_uv);" +
            "  if (center.a > 0.1) { gl_FragColor = center; return; }" +
            "  float ox = u_outline_width * u_texel.x;" +
            "  float oy = u_outline_width * u_texel.y;" +
            "  float m = 0.0;" +
            "  m = max(m, texture2D(u_tex, v_uv + vec2( ox, 0.0)).a);" +
            "  m = max(m, texture2D(u_tex, v_uv + vec2(-ox, 0.0)).a);" +
            "  m = max(m, texture2D(u_tex, v_uv + vec2(0.0,  oy)).a);" +
            "  m = max(m, texture2D(u_tex, v_uv + vec2(0.0, -oy)).a);" +
            "  m = max(m, texture2D(u_tex, v_uv + vec2( ox,  oy)).a);" +
            "  m = max(m, texture2D(u_tex, v_uv + vec2( ox, -oy)).a);" +
            "  m = max(m, texture2D(u_tex, v_uv + vec2(-ox,  oy)).a);" +
            "  m = max(m, texture2D(u_tex, v_uv + vec2(-ox, -oy)).a);" +
            "  if (m > 0.1) { gl_FragColor = u_outline_color; return; }" +
            "  discard;" +
            "}";

        function compile(type, src) {
            var s = gl.createShader(type);
            gl.shaderSource(s, src);
            gl.compileShader(s);
            return s;
        }

        var program = gl.createProgram();
        gl.attachShader(program, compile(gl.VERTEX_SHADER, vertSrc));
        gl.attachShader(program, compile(gl.FRAGMENT_SHADER, fragSrc));
        gl.linkProgram(program);
        gl.useProgram(program);

        var buffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
        gl.bufferData(gl.ARRAY_BUFFER,
            new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
        var aPos = gl.getAttribLocation(program, "a_pos");
        gl.enableVertexAttribArray(aPos);
        gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);

        gl.enable(gl.BLEND);
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

        gl.uniform4f(gl.getUniformLocation(program, "u_outline_color"), 1.0, 0.0, 0.0, 1.0);
        gl.uniform1f(gl.getUniformLocation(program, "u_outline_width"), 2.0);
        var uTexel = gl.getUniformLocation(program, "u_texel");

        var paths = [
            "media/player/HJ_run01.png",
            "media/player/HJ_run02.png",
            "media/player/HJ_run03.png",
            "media/player/HJ_run04.png",
            "media/player/HJ_run05.png",
            "media/player/HJ_run06.png",
            "media/player/HJ_run07.png",
            "media/player/HJ_run08.png",
            "media/player/HJ_run09.png",
            "media/player/HJ_run010.png",
        ];

        var textures = [];
        var size = null;
        var loaded = 0;

        paths.forEach(function (path, i) {
            var image = new Image();
            image.onload = function () {
                var tex = gl.createTexture();
                gl.bindTexture(gl.TEXTURE_2D, tex);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
                textures[i] = tex;
                if (!size) size = { w: image.width, h: image.height };

                if (++loaded === paths.length) {
                    canvas.width = size.w;
                    canvas.height = size.h;
                    gl.viewport(0, 0, canvas.width, canvas.height);
                    gl.uniform2f(uTexel, 1.0 / size.w, 1.0 / size.h);

                    var index = 0;
                    var fps = 15;
                    function draw() {
                        gl.bindTexture(gl.TEXTURE_2D, textures[index]);
                        gl.clearColor(0, 0, 0, 0);
                        gl.clear(gl.COLOR_BUFFER_BIT);
                        gl.drawArrays(gl.TRIANGLES, 0, 6);
                    }
                    draw();
                    setInterval(function () {
                        index = (index + 1) % textures.length;
                        draw();
                    }, 1000 / fps);
                }
            };
            image.src = path;
        });
    }

    // --- Healthbar (mirrors Player:set_health; fill width animates via CSS transition) ---
    function startHealthbarDemo() {
        var fill = document.getElementById("hb-fill");
        var text = document.getElementById("hb-text");
        if (!fill || !text) return;

        var maxHealth = 100;
        var health = maxHealth;
        var draining = true;

        function setHealth(value) {
            health = Math.max(0, Math.min(maxHealth, value));
            var percent = Math.floor((health / maxHealth) * 100);
            fill.style.width = percent + "%";
            text.textContent = Math.floor(health) + " / " + maxHealth;
        }

        setHealth(maxHealth);

        // Tick every 0.7s: take damage down to 0, then heal back to full, and repeat.
        setInterval(function () {
            if (draining) {
                setHealth(health - 15);
                if (health <= 0) draining = false;
            } else {
                setHealth(health + 20);
                if (health >= maxHealth) draining = true;
            }
        }, 700);
    }

    startSpriteDemos();
    startShaderDemo();
    startRunOutlineDemo();
    startHealthbarDemo();
})();
