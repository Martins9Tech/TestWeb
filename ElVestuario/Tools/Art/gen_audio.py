"""
Sintetiza los efectos de sonido de "El Vestuario" (WAV mono, 44.1 kHz, 16 bits).

Uso:  python3 gen_audio.py
Salida: ../../SourceArt/Audio/S_*.wav

Son sonidos provisionales generados por codigo. Para la version final conviene grabarlos
o usar librerias (Freesound con licencia CC0, Sonniss GDC bundle, etc.) con el MISMO nombre.
"""
import os
import wave

import numpy as np

SR = 44100
OUT_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "SourceArt", "Audio"))
rng = np.random.default_rng(1234)


def t_axis(dur):
    return np.arange(int(SR * dur)) / SR


def noise(dur):
    return rng.uniform(-1, 1, int(SR * dur))


def band(sig, lo, hi):
    """Filtro paso banda por FFT (lo/hi en Hz)."""
    spec = np.fft.rfft(sig)
    f = np.fft.rfftfreq(len(sig), 1 / SR)
    mask = ((f >= lo) & (f <= hi)).astype(float)
    # Bordes suaves para evitar "ringing"
    mask = np.convolve(mask, np.hanning(31) / np.hanning(31).sum(), mode="same")
    return np.fft.irfft(spec * mask, len(sig))


def env(dur, attack, decay_rate):
    t = t_axis(dur)
    a = np.clip(t / max(attack, 1e-4), 0, 1)
    return a * np.exp(-t * decay_rate)


def norm(sig, peak=0.9):
    m = np.max(np.abs(sig)) or 1.0
    return sig / m * peak


def loopable(sig, fade=0.25):
    """Funde el final con el principio para que el bucle no haga 'clic'."""
    n = int(SR * fade)
    head, tail = sig[:n].copy(), sig[-n:].copy()
    w = np.linspace(0, 1, n)
    out = sig[:-n].copy()
    out[:n] = head * w + tail * (1 - w)
    return out


def write(name, sig, peak=0.9):
    os.makedirs(OUT_DIR, exist_ok=True)
    data = (norm(sig, peak) * 32767).astype(np.int16)
    with wave.open(os.path.join(OUT_DIR, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())
    print(f"ok {name:<16} {len(sig) / SR:5.2f}s")


def thud(dur, freq, noise_lo, noise_hi, decay, click=0.3):
    t = t_axis(dur)
    body = np.sin(2 * np.pi * freq * t * (1 - 0.3 * t / dur)) * env(dur, 0.002, decay)
    grit = band(noise(dur), noise_lo, noise_hi) * env(dur, 0.001, decay * 2.5) * click * 4
    return body + grit


def s_footstep():
    return thud(0.22, 90, 800, 5000, 30, click=0.5)


def s_enemy_step():
    t = t_axis(0.5)
    base = thud(0.5, 55, 150, 1800, 12, click=0.6)
    squelch = band(noise(0.5), 300, 1200) * env(0.5, 0.03, 9) * np.sin(2 * np.pi * 7 * t) ** 2 * 3
    return base + squelch


def s_enemy_breath():
    dur = 3.4
    t = t_axis(dur)
    cycle = np.sin(np.pi * t / (dur / 2)) ** 2          # inhalar / exhalar
    rasp = band(noise(dur), 350, 2600) * cycle
    growl_f = 62 + 6 * np.sin(2 * np.pi * 0.7 * t)
    growl = np.sign(np.sin(2 * np.pi * np.cumsum(growl_f) / SR)) * 0.25
    growl = band(growl, 40, 900) * (cycle ** 3) * (t > dur / 2)
    wet = band(noise(dur), 2000, 6000) * (rng.random(len(t)) > 0.9985) * 6
    return loopable(rasp + growl * 2.0 + wet)


def s_enemy_scream():
    dur = 1.8
    t = t_axis(dur)
    out = np.zeros_like(t)
    for detune in (0.97, 1.0, 1.035, 1.5, 2.02):
        f = (420 + 900 * np.exp(-t * 2.5) + 60 * np.sin(2 * np.pi * 9 * t)) * detune
        phase = 2 * np.pi * np.cumsum(f) / SR
        out += (2 * ((phase / (2 * np.pi)) % 1) - 1) * (1 / detune)
    out += band(noise(dur), 1500, 9000) * 1.2
    out = np.tanh(out * 2.5)
    return out * env(dur, 0.02, 1.3)


def s_knock():
    out = np.zeros(int(SR * 1.2))
    for start in (0.0, 0.28, 0.5):
        k = thud(0.35, 120, 400, 3500, 22, click=0.8)
        i = int(start * SR)
        out[i:i + len(k)] += k
    return out


def s_spark():
    dur = 1.1
    t = t_axis(dur)
    crackle = band(noise(dur), 1500, 12000) * (rng.random(len(t)) > 0.97) * 3
    crackle += band(noise(dur), 3000, 10000) * env(dur, 0.001, 18) * 2
    buzz = np.sign(np.sin(2 * np.pi * 100 * t)) * 0.4 * env(dur, 0.001, 3)
    pop = thud(dur, 70, 200, 4000, 25, click=1.0)
    return crackle * env(dur, 0.001, 3.5) + band(buzz, 80, 3000) + pop


def s_fuse_click():
    return thud(0.25, 400, 2000, 9000, 40, click=1.2) + np.concatenate(
        [np.zeros(int(0.06 * SR)), thud(0.19, 180, 800, 5000, 35, click=0.8)])


def s_pickup():
    t = t_axis(0.35)
    ring = sum(np.sin(2 * np.pi * f * t) / (i + 1) for i, f in enumerate((1850, 2710, 4020)))
    return ring * env(0.35, 0.001, 14) + band(noise(0.35), 3000, 9000) * env(0.35, 0.001, 60)


def creak(dur, f0, f1, rough=0.6):
    t = t_axis(dur)
    f = np.linspace(f0, f1, len(t)) * (1 + 0.08 * np.sin(2 * np.pi * 5.3 * t))
    stick = (rng.random(len(t)) > 0.5).astype(float)
    phase = 2 * np.pi * np.cumsum(f) / SR
    saw = 2 * ((phase / (2 * np.pi)) % 1) - 1
    s = band(saw * (1 - rough + rough * stick), 150, 4000)
    return s * np.sin(np.pi * t / dur) ** 0.7


def s_locker_creak():
    return creak(0.9, 380, 260) + np.concatenate([np.zeros(int(0.8 * SR)), thud(0.3, 140, 500, 4000, 20)])[:int(0.9 * SR)] * 0.6


def s_door_open():
    lock = thud(0.4, 90, 300, 5000, 18, click=1.0)
    c = creak(2.2, 180, 110, rough=0.8)
    out = np.concatenate([lock, c])
    return out


def s_heartbeat():
    dur = 1.0
    out = np.zeros(int(SR * dur))
    for start, amp in ((0.0, 1.0), (0.22, 0.7)):
        b = thud(0.3, 48, 20, 200, 16, click=0.1) * amp
        i = int(start * SR)
        out[i:i + len(b)] += b
    return band(out, 20, 400)


def s_drip():
    t = t_axis(0.5)
    f = 900 + 1400 * np.exp(-t * 40)
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * env(0.5, 0.0005, 18)
    echo = np.zeros_like(s)
    for d, a in ((0.09, 0.35), (0.19, 0.2), (0.31, 0.1)):
        i = int(d * SR)
        echo[i:] += s[:-i] * a
    return s + echo


def s_jumpscare():
    dur = 2.0
    t = t_axis(dur)
    chord = sum(np.sin(2 * np.pi * f * t + np.sin(2 * np.pi * 3 * t) * 2) for f in (98, 104, 147, 208, 311))
    hit = band(noise(dur), 60, 8000) * env(dur, 0.001, 5) * 3
    return np.tanh((chord * env(dur, 0.005, 1.6) + hit) * 1.8) + s_enemy_scream_pad(dur)


def s_enemy_scream_pad(dur):
    sc = s_enemy_scream()
    out = np.zeros(int(SR * dur))
    out[:len(sc)] += sc[:len(out)] * 0.6
    return out


def s_power_on():
    dur = 2.6
    t = t_axis(dur)
    clunk = thud(dur, 60, 200, 3000, 10, click=1.2)
    rise = np.clip(t / 1.2, 0, 1)
    hum = (np.sin(2 * np.pi * 50 * t) + 0.5 * np.sin(2 * np.pi * 100 * t) + 0.25 * np.sign(np.sin(2 * np.pi * 150 * t))) * rise * 0.5
    buzz = band(noise(dur), 4000, 9000) * rise * 0.15
    return clunk + hum + buzz


def s_ambience():
    dur = 24.0
    t = t_axis(dur)
    rumble = band(noise(dur), 25, 140) * (0.7 + 0.3 * np.sin(2 * np.pi * t / 12)) * 3
    drone = sum(np.sin(2 * np.pi * f * t + 0.3 * np.sin(2 * np.pi * 0.05 * t * (i + 1)))
                for i, f in enumerate((41.2, 41.9, 61.7, 82.1)))
    air = band(noise(dur), 300, 2500) * (0.5 + 0.5 * np.sin(2 * np.pi * t / 8 + 1)) * 0.25
    # Base simetrica para que el bucle de 24 s encaje (periodos de 12 s y 8 s)
    return loopable(rumble + drone * 0.35 + air, fade=1.0)


def s_whisper():
    dur = 2.2
    t = t_axis(dur)
    out = np.zeros_like(t)
    # "Silabas": rafagas de ruido con formantes cambiantes
    for k in range(7):
        start = 0.15 + k * 0.27 + rng.uniform(-0.03, 0.03)
        length = rng.uniform(0.12, 0.22)
        lo = rng.choice([500, 700, 900, 1200])
        seg = band(noise(length), lo, lo * 3.2) * np.hanning(int(SR * length))
        i = int(start * SR)
        out[i:i + len(seg)] += seg[:len(out) - i]
    out += band(noise(dur), 3000, 7000) * 0.15 * np.sin(np.pi * t / dur)
    return out


def main():
    sounds = {
        "S_Footstep": s_footstep(),
        "S_EnemyStep": s_enemy_step(),
        "S_EnemyBreath": s_enemy_breath(),
        "S_EnemyScream": s_enemy_scream(),
        "S_Knock": s_knock(),
        "S_Spark": s_spark(),
        "S_FuseClick": s_fuse_click(),
        "S_Pickup": s_pickup(),
        "S_LockerCreak": s_locker_creak(),
        "S_DoorOpen": s_door_open(),
        "S_Heartbeat": s_heartbeat(),
        "S_Drip": s_drip(),
        "S_Jumpscare": s_jumpscare(),
        "S_PowerOn": s_power_on(),
        "S_Ambience": s_ambience(),
        "S_Whisper": s_whisper(),
    }
    for name, sig in sounds.items():
        write(name, sig)


if __name__ == "__main__":
    main()
