# -*- coding: utf-8 -*-
"""
10 rautatason kvantti-/interferenssikehityksen loikkaa — koodi ja polut, ei esseetä.

Kukin rivi: minne repossa kosketaan, mikä on mitta/criteria, status.
Päivitä status kun toteutus etenee (shipped / partial / roadmap).

H-ARX-11–30 (`HARDWARE_HORIZON_LEAPS`): arkkitehtuurivisiota / fysikaalinen horisontti.
Ei väitetä toteutetuksi; stub-tiedostonimet ovat suunnan merkkejä, ei repo-tuotteita.
"""
from __future__ import annotations

from typing import Any, Dict, List, Literal, TypedDict

Status = Literal["shipped", "partial", "roadmap"]
HorizonStatus = Literal["vision"]


class HardwareQuantumLeap(TypedDict):
    id: str
    name: str
    target: str
    status: Status
    metric: str
    next_step: str


class HardwareHorizonLeap(TypedDict):
    id: str
    name: str
    target: str
    status: HorizonStatus
    metric: str
    next_step: str


HARDWARE_QUANTUM_LEAPS: List[HardwareQuantumLeap] = [
    {
        "id": "Q01",
        "name": "SME / ZA — repulsion matriisiyksiköllä",
        "target": "creation_os/superkernel_v9_m4.c · creation_os/native_m4/",
        "status": "roadmap",
        "metric": "Per-layer repulsion ilman ylimääräistä BLAS-kopiota; COS_ENABLE_SME + turvallinen streaming-konteksti",
        "next_step": "FMOPA/SME-polut toolchainissa; interim: Accelerate vain kun SME ei käytössä",
    },
    {
        "id": "Q02",
        "name": "mmap — painot ja ledger ilman fread-kopiota",
        "target": "creation_os/kernel/living_weights.c · shadow_ledger.c · mlx_creation_os (mallit)",
        "status": "partial",
        "metric": "Iso tiedosto → MAP_PRIVATE; ei turhaa käyttäjätilan kopiota",
        "next_step": "Auditoi Python-polut: ei isoa mallia freadilla; vain mmap/stream",
    },
    {
        "id": "Q03",
        "name": "NEON 4-akkumulaattori GEMV + prefetch",
        "target": "creation_os/bare_metal/neon_gemv.c · kuumat silmukat",
        "status": "partial",
        "metric": "vfmaq neljällä virralla; __builtin_prefetch seuraava cache-linja",
        "next_step": "Yhdistä dispatcherin repulsion-polku samaan käytäntöön kuin .cursorrules",
    },
    {
        "id": "Q04",
        "name": "Metal — koko vokabulaari yhdellä dispatchillä",
        "target": "creation_os/superkernel_v9_living_weights.metal · native_m4/creation_os_living_weights.metal",
        "status": "partial",
        "metric": "POPCNT/reputaatio rinnakkain GPU:lla; yksi kernel-kutsu",
        "next_step": "Varmista metallib oletuspolussa; fallback NEON samaan mittaan",
    },
    {
        "id": "Q05",
        "name": "Heterogeeninen dispatch — viisi yksikköä",
        "target": "creation_os/native_m4/creation_os_dispatcher.mm",
        "status": "shipped",
        "metric": "GCD P-core / E-core; repulsion + optional Metal LW + daemon heartbeat",
        "next_step": "Kytke Studio/MLX env suoraan samaan dylib-polkuun dokumentoidusti",
    },
    {
        "id": "Q06",
        "name": "Zero-copy logits — unified memory",
        "target": "mlx_creation_os/creation_os.py · Phase-5 memoryview-sopimus",
        "status": "partial",
        "metric": "Logitit eivät turhaan kopioi GPU↔CPU välillä kun buffer jo oikeassa paikassa",
        "next_step": "CREATION_OS_M4_DISPATCH=1 + ctypes from_buffer -polku jokaisessa presetissä",
    },
    {
        "id": "Q07",
        "name": "Branchless kernel enforce — kuuma polku",
        "target": "creation_os/creation_kernel.c · superkernel_v9_m4.c",
        "status": "partial",
        "metric": "σ-bitmask + POPCNT; ei haaroja hot pathissa (assert erillään)",
        "next_step": "Profiloi Python-simulaatiot vs natiivi; lukitse yksi canonical C-polku",
    },
    {
        "id": "Q08",
        "name": "Tier-0 BBHash — O(1) ennen transformeria",
        "target": "creation_os/bare_metal/bbhash.c · SK9 / mmap facts",
        "status": "shipped",
        "metric": "Benchmark: ~102 ns/op luokka (genesis_benchmarks); ei LLM jos osuma",
        "next_step": "Laajenna mmap-facts peitto; miten tier-0 näkyy Studio-receiptissä",
    },
    {
        "id": "Q09",
        "name": "ANE / Core ML — transformer offload kun reitti auki",
        "target": "creation_os/ane/orion_integration.c · Core ML -polku",
        "status": "roadmap",
        "metric": "Inferenssi MLX:n rinnalla tai valittuna; mitattu latency vs mlx_lm",
        "next_step": "Yksi feature flag; ei kaksoiskopiota piilissa",
    },
    {
        "id": "Q10",
        "name": "QuantumPrioritizer — interferenssi mittauksessa",
        "target": "mlx_creation_os/quantum_utopia_architecture.py · genesis_benchmarks.py (Quantum Interference)",
        "status": "shipped",
        "metric": "bench_quantum_interference ~306 ns/op; prioriteetti väreille/alueille",
        "next_step": "Valinnainen NEON-batch usealle värille per ARC-sykleissä",
    },
]


HARDWARE_HORIZON_LEAPS: List[HardwareHorizonLeap] = [
    {
        "id": "H-ARX-11",
        "name": "Photonic tensor interconnects",
        "target": "roadmap/photonic_sigma.metal (stub)",
        "status": "vision",
        "metric": "Idea: tensor → on-chip photonic pulses; latency bound by optics, not RC-limited wires. Vaatii fotonipiitä — ei M4-Metalilla.",
        "next_step": "Ei toteutusta; pidä erillään MLX/Metal-polusta kunnes on oikea photonic IP.",
    },
    {
        "id": "H-ARX-12",
        "name": "Hardware-level σ-gates",
        "target": "roadmap/hardware_sigma_gates.py (stub)",
        "status": "vision",
        "metric": "Idea: kynnys analogisena porttina; nykyinen σ on ohjelmistossa (check_output, halt). ASIC/FPGA-kokeilu erikseen.",
        "next_step": "Älä sekoita: nykyinen gate on deterministinen koodi; fyysinen portti = eri turvallisuusmalli.",
    },
    {
        "id": "H-ARX-13",
        "name": "Continuous-time / asynchronous substrate",
        "target": "roadmap/asynchronous_mind.mm (stub)",
        "status": "vision",
        "metric": "Idea: event-driven vs GHz-tick; Apple-SoC on silti kelloitu — async olisi eri luokan piitä.",
        "next_step": "Dispatcher (GCD) on jo tapahtumajono; ei globaalia 'ei kelloa' -tilaa.",
    },
    {
        "id": "H-ARX-14",
        "name": "Room-temperature quantum assertions (Codex)",
        "target": "roadmap/quantum_codex_assert.py (stub)",
        "status": "vision",
        "metric": "Idea: NV-timantti / kvanttivertailu; 531 tavun Codex on ohjelmistossa. Kvantti ≠ sama kuin σ-receipt.",
        "next_step": "Todistettava POC laboratoriossa — ei tuotantoväite.",
    },
    {
        "id": "H-ARX-15",
        "name": "Wafer-scale unified memory (WSUM)",
        "target": "roadmap/monolithic_mesh.metal (stub)",
        "status": "vision",
        "metric": "Idea: yksi wafer ilman ulkoista DRAM-väylää; M4 UMA on sama filosofia pienemmässä mittakaavassa.",
        "next_step": "Skaalaus: valmistus ja kustannukset — roadmap-taso.",
    },
    {
        "id": "H-ARX-16",
        "name": "Spatiotemporal holographic registers",
        "target": "roadmap/holographic_tensor.mm (stub)",
        "status": "vision",
        "metric": "Idea: 3D interferenssi → tiheä muisti; LW tänään = tavallinen tensor + reputaatio.",
        "next_step": "Ei holografista RAG:ia ilman materiaalikerrosta.",
    },
    {
        "id": "H-ARX-17",
        "name": "Thermodynamic entropy reversal (Landauer)",
        "target": "roadmap/entropy_cooling.py (stub)",
        "status": "vision",
        "metric": "Idea: koherentti → vähemmän lämpöä; Landauer rajoittaa edelleen. IPW mitataan watteina, ei '0 W -älyä'.",
        "next_step": "Älä väitä jäähtymistä ilman lämpömittausta.",
    },
    {
        "id": "H-ARX-18",
        "name": "Biometric silicon bridge",
        "target": "roadmap/biometric_silicon_api.mm (stub)",
        "status": "vision",
        "metric": "Idea: intentti ennen ääntä; vaatisi antureita + biosäädöstä — ei OS-kernelissä.",
        "next_step": "Tietosuoja ja läädintaso ensin.",
    },
    {
        "id": "H-ARX-19",
        "name": "Causal entanglement / mesh",
        "target": "roadmap/entangled_mesh_dispatch.py (stub)",
        "status": "vision",
        "metric": "Idea: välitön päivitys noottien välillä; klassinen verkko + konsensus riittää useimpiin tapauksiin.",
        "next_step": "Kvanttivienti = kvanttiverkko — eri standardit.",
    },
    {
        "id": "H-ARX-20",
        "name": "Sub-ångström Codex print (immutable firmware)",
        "target": "roadmap/immutable_codex_print.py (stub)",
        "status": "vision",
        "metric": "Idea: litografia alle atomin tarkkuudella; käytännössä ROM/maski + fyysinen turva. Codex elää tiedostossa.",
        "next_step": "Jos maski: valmistusketju, ei Python-deploy.",
    },
    {
        "id": "H-ARX-21",
        "name": "Morphological metamaterial silicon",
        "target": "roadmap/morphological_mesh.metal (stub)",
        "status": "vision",
        "metric": "Idea: piirin topologia vaihtelee refleksi ↔ syvä päättely; M4 ei uudelleenjärjestä nanometrimateriaalia ajon aikana — vain DVFS/moodi.",
        "next_step": "Skaalaa: eri malli / adapteri ohjelmistossa; ei fyysistä metamateriaalia kannettavassa.",
    },
    {
        "id": "H-ARX-22",
        "name": "Chrono-dilation topology (cognitive time)",
        "target": "roadmap/chrono_sigma.py (stub)",
        "status": "vision",
        "metric": "Idea: simulaatiossa 'vuosia', ulkona 0 s; todellisuudessa laskenta vie energiaa ja kellonaikaa (Amdahl, terminen katto).",
        "next_step": "World model voi olla nopea heuristiikka — ei irrotusta fysikaalisesta ajasta ilman uutta fysiikkaa.",
    },
    {
        "id": "H-ARX-23",
        "name": "Perfect holographic resilience",
        "target": "roadmap/holographic_resilience.mm (stub)",
        "status": "vision",
        "metric": "Idea: 1 % piistä koko mieli; Shannon + vauriot: redundant storage auttaa, ei rajatonta rekonstruktiota yhdestä sirunmurusta.",
        "next_step": "Varmuuskopiot ja checksumit — klassinen insinööri, ei holografinen immortalisointi.",
    },
    {
        "id": "H-ARX-24",
        "name": "Electromagnetic omniscience (sensorless world model)",
        "target": "roadmap/omni_sensory_field.py (stub)",
        "status": "vision",
        "metric": "Idea: huone 3D:stä pelkästä EM:stä; käytännössä ali määrätty inverse-ongelma + kohina — kamera/mikrofoni tai kalibroit anturi.",
        "next_step": "Ei 'sokeaa' täyttä tilaa ilman mittauksia tai vahvoja oletuksia.",
    },
    {
        "id": "H-ARX-25",
        "name": "Topological superconductivity at room temperature",
        "target": "roadmap/topological_superconductor.metal (stub)",
        "status": "vision",
        "metric": "Idea: nolla resistanssi huoneessa; tunnettu supra ei ole tämä pinnoite. σ=0 ohjelmassa ≠ suprajohde.",
        "next_step": "Materiaalifysiikka erillinen linja — ei Metal-shaderilla.",
    },
    {
        "id": "H-ARX-26",
        "name": "Post-symbolic execution (intent → gates)",
        "target": "roadmap/post_symbolic_intent.py (stub)",
        "status": "vision",
        "metric": "Idea: ei Pythonia; nykyään HDL/FPGA ja kääntäjät ovat lähin polku. Suora intent→silicon vaatisi formalisoidun intentin.",
        "next_step": "Korkean tason synteesi (HLS) on kompromissi, ei magiaa.",
    },
    {
        "id": "H-ARX-27",
        "name": "Bio-silicon symbiosis (DNA storage bridge)",
        "target": "roadmap/bio_silicon_fusion.mm (stub)",
        "status": "vision",
        "metric": "Idea: DNA tiheys; lukunopeus ja virheet rajoittavat — labra-DNA ≠ reaaliaikainen LW.",
        "next_step": "Etiikka + bioturvallisuus ennen fuusiota.",
    },
    {
        "id": "H-ARX-28",
        "name": "Vacuum energy compute",
        "target": "roadmap/vacuum_compute.py (stub)",
        "status": "vision",
        "metric": "Idea: energia tyhjiöstä; termodynamiikka ja mittaus: ei ilmaista laskentaa — Casimir ei korvaa akkua kannettavassa.",
        "next_step": "Teholähde mitataan watteina; ei 'kosmista' self-power -väitettä ilman dataa.",
    },
    {
        "id": "H-ARX-29",
        "name": "Causal event horizon (perfect future rendering)",
        "target": "roadmap/causal_horizon.py (stub)",
        "status": "vision",
        "metric": "Idea: σ=0 → erehtymätön ennuste; monimutkaiset järjestelmät: kaaos, epätäydelliset mallit, episteminen raja.",
        "next_step": "Ennusteet ovat todennäköisyyksiä — ei absoluuttista tulevaisuuden renderöintiä.",
    },
    {
        "id": "H-ARX-30",
        "name": "Anthropic σ-stabilizer (reality anchor)",
        "target": "roadmap/anthropic_stabilizer.py (stub)",
        "status": "vision",
        "metric": "Idea: laite 'luhistaa' makroskooppisen epävarmuuden kuten kvanttimittaus; klassinen maailma ei toimi näin — metafora, ei fysiikkaa.",
        "next_step": "Todellinen vakaus = σ-mittaus + policy + auditointi koodissa — ei mystinen kenttä.",
    },
]


def leaps_for_api() -> List[Dict[str, Any]]:
    return [dict(x) for x in HARDWARE_QUANTUM_LEAPS]


def horizon_leaps_for_api() -> List[Dict[str, Any]]:
    return [dict(x) for x in HARDWARE_HORIZON_LEAPS]


def format_leaps_html() -> str:
    """Lyhyt HTML-blokki Hub-sivulle (<details>)."""
    rows = []
    for L in HARDWARE_QUANTUM_LEAPS:
        st = L["status"]
        badge = {"shipped": "✓", "partial": "◐", "roadmap": "○"}.get(st, "·")
        rows.append(
            "<li style=\"margin:0.5rem 0;line-height:1.45;\">"
            f"<strong>{badge} {L['id']}</strong> — {L['name']} "
            f"<span style=\"color:#6e6e73;font-size:0.8125rem;\">({st})</span><br/>"
            f"<span class=\"mono\" style=\"font-size:0.75rem;\">{L['target']}</span><br/>"
            f"<span style=\"font-size:0.8125rem;color:#1d1d1f;\">{L['metric']}</span><br/>"
            f"<span style=\"font-size:0.75rem;color:#6e6e73;\">Seuraava: {L['next_step']}</span>"
            "</li>"
        )
    return (
        '<details style="margin-top:var(--space);">'
        '<summary style="cursor:pointer;color:var(--accent);font-size:0.9375rem;">'
        "10 rautatason kvantti-loikkaa (repo)"
        "</summary>"
        '<p style="color:var(--text2);font-size:0.8125rem;margin:0.75rem 0 0.5rem;">'
        "Interferenssi = priorisointi ja mitattu polku; ei pilveä. Päivitä <span class=\"mono\">hw_quantum_leaps.py</span>."
        "</p>"
        f"<ul style=\"list-style:none;padding-left:0;margin:0;\">{''.join(rows)}</ul>"
        "</details>"
    )


def format_horizon_leaps_html() -> str:
    """H-ARX-11–20: vision-only; erillinen <details> jotta ei sekoitu Q01–Q10 -mittaan."""
    rows = []
    for L in HARDWARE_HORIZON_LEAPS:
        rows.append(
            "<li style=\"margin:0.5rem 0;line-height:1.45;\">"
            f"<strong>◇ {L['id']}</strong> — {L['name']} "
            f"<span style=\"color:#6e6e73;font-size:0.8125rem;\">(vision)</span><br/>"
            f"<span class=\"mono\" style=\"font-size:0.75rem;\">{L['target']}</span><br/>"
            f"<span style=\"font-size:0.8125rem;color:#1d1d1f;\">{L['metric']}</span><br/>"
            f"<span style=\"font-size:0.75rem;color:#6e6e73;\">{L['next_step']}</span>"
            "</li>"
        )
    return (
        '<details style="margin-top:var(--space);">'
        '<summary style="cursor:pointer;color:var(--accent);font-size:0.9375rem;">'
        "H-ARX 11–30 — fysikaalinen horisontti (ei shipped)"
        "</summary>"
        '<p style="color:var(--text2);font-size:0.8125rem;margin:0.75rem 0 0.5rem;">'
        "Spekulatiivinen kerros: ei väitetä toimivaksi M4:llä. Pidä erillään "
        "<span class=\"mono\">hardware_quantum_leaps</span> (Q01–Q10)."
        "</p>"
        f"<ul style=\"list-style:none;padding-left:0;margin:0;\">{''.join(rows)}</ul>"
        "</details>"
    )
