# 게임 치장템 자동생성 파이프라인 (Gemma + SDXL + BRIA)

캐릭터 치장 아이템(모자/옷/무기/이펙트 아이콘)을 **투명배경 PNG**로 뽑는 ComfyUI 파이프라인.
구조: 짧은 한 줄 요청 → Gemma가 상세 영문 프롬프트로 강화 → SDXL 이미지 생성 → BRIA 누끼 → 저장.

---

## 1. 파이프라인 구조

```
[12] 시스템 프롬프트 ─┐
                     ├─[2] 합치기 ─[3] Gemma 생성 ─[7][8] ``` 제거 ─[4] JSON에서 prompt 추출
[1] 유저 요청 ───────┘                                                        │
                                                                              ▼
[20] SDXL 체크포인트 ─ CLIP ─[21] 긍정 프롬프트 ◀── (강화된 프롬프트 자동 연결)
                                     │
            [22] 부정 프롬프트(고정) ─┤
            [23] 빈 잠재이미지 ───────┤
                                     ▼
                                [24] KSampler ─[25] VAE디코드 ─[27] BRIA 누끼 ─[28] 저장
                                                                  ▲
                                               [26] BRIA 모델로더 ┘
```

핵심: Gemma는 "다양성/상세화" 담당, 고정 부정프롬프트 + 시스템프롬프트는 "스타일 고정" 담당.

---

## 2. 사전 준비 (설치)

### 2-1. ComfyUI 최신 버전 필수
TextGenerate / JsonExtractString / RegexReplace 는 **ComfyUI 코어 내장 노드**라 커스텀 노드팩 설치 불필요.
단, 비교적 최근 추가된 기능이라 ComfyUI가 최신이어야 함.
```bash
cd ~/ComfyUI && git pull
```

### 2-2. BRIA 누끼 커스텀 노드 (이것만 설치 필요)
```bash
cd ~/ComfyUI/custom_nodes
git clone https://github.com/ZHO-ZHO-ZHO/ComfyUI-BRIA_AI-RMBG.git
~/comfyui-env/bin/pip install transformers
# BRIA 모델 파일 다운로드
cd ComfyUI-BRIA_AI-RMBG
mkdir -p RMBG-1.4
wget -O RMBG-1.4/model.pth "https://huggingface.co/briaai/RMBG-1.4/resolve/main/model.pth"
```

### 2-3. 모델 파일 3개 다운로드 (경로 주의)

| 모델 | 다운로드 위치 | 폴더 |
|------|--------------|------|
| Gemma 4 E2B (프롬프트 강화용) | huggingface.co/Comfy-Org/gemma-4 | `models/text_encoders/` |
| SDXL 체크포인트 (Game Icon Institute V4_XL) | civitai.com/models/47800 | `models/checkpoints/` |
| BRIA RMBG-1.4 (누끼) | 위 2-2에서 자동 | (자동) |

Gemma E2B 받기:
```bash
cd ~/ComfyUI/models/text_encoders
wget -O gemma4_e2b_it_bf16.safetensors "https://huggingface.co/Comfy-Org/gemma-4/resolve/main/text_encoders/gemma4_e2b_it_bf16.safetensors"
```
(VRAM 여유 있으면 더 똑똑한 E4B fp8(9GB)도 가능: `Comfy-Org/Gemma4` 리포 — 대문자 주의)

---

## 3. 사용법

1. `gemma_sdxl_bria_merged.json` 로드
2. **[1] 유저 요청 노드**의 `replace` 칸에 원하는 걸 입력
   - 예: `Generate one random cute hat accessory worn on a character's head.`
   - 예: `Generate one random fun cartoon weapon prop.`
   - 예: `Generate one random outfit costume.`
3. 실행 → `output/` 에 `item_XXXXX_.png` (투명배경) 생성
4. 같은 요청으로 여러 번 돌리면 Gemma가 매번 다른 아이템을 뽑아줌 (시드 랜덤)

### 스타일 바꾸기
- **[12] REWRITE_SYSTEM_PROMPT 노드**의 `FIXED VISUAL STYLE` 줄 키워드만 수정.
- 더 입체로: `matte` → `glossy`, `minimal detail` → `more detail`
- JSON 출력 형식(`{"prompt": "..."}`)과 `<|turn>` 턴 토큰은 절대 건드리지 말 것 (파싱 깨짐).

---

## 4. ⚠️ 검증 포인트 (TA 확인 필요)

1. **[11] CLIPLoader의 `type` 값** — 현재 `stable_diffusion`으로 설정됨 (기존 작동 워크플로우 기준).
   만약 Gemma 로드 에러 나면, 이 노드 드롭다운에서 다른 type 옵션(gemma 관련) 있는지 확인.
   가장 확실한 방법: ComfyUI 기본 제공 **Gemma 4 템플릿 워크플로우**를 한 번 로드해서
   거기 CLIPLoader가 쓰는 type 값을 그대로 복사.

2. **포맷** — 이 파일은 **API 포맷**(`/prompt` 엔드포인트 / 스크립트 실행용).
   GUI에서 그냥 드래그로 안 열릴 수 있음. GUI에서 시각적으로 편집하려면:
   - 공식 Gemma4 템플릿(앞단) + 기존 BRIA 워크플로우(뒷단)를 각각 GUI로 열어
     `JsonExtractString` 출력 → `CLIPTextEncode(긍정)` text 입력으로 연결.

3. **8GB VRAM 주의** — SDXL + Gemma 동시 로드 시 메모리 빡빡함.
   `--lowvram` 유지. Gemma 생성이 느리면 정상(반자동이라 OK). 터지면 E2B 유지 + SDXL 해상도 1024 고정.

4. **모델 파일명** — `[20]` ckpt_name, `[11]` clip_name 이 실제 다운로드한 파일명과 일치하는지 확인.
```
