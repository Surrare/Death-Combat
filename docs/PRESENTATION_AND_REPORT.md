# Презентация и технический отчёт: DEATH COMBAT (Group: IT-2502)

- **PowerPoint (`.pptx`, 10 слайдов 16:9 со встроенными скриншотами):** `Death_Combat_Presentation_IT2502.pptx`
- **Интерактивная веб-презентация (`.html`, открывается в любом браузере, клавиши `←`/`→`/`Space`, `F` — полный экран):** `docs/presentation.html`

---

## Слайд 1 — Титульный слайд (Project Identification)

- **Университет и дисциплина:** Astana IT University — *Computer Graphics Fundamentals (Final Project, 2026–2027)*
- **Название темы (Topic Title):** **Death Combat** — *Real-Time 3D Tactical Arena Fighter & Labyrinth Hunt Engine*
- **Команда и группа:**
  - **Ali Yerzhanuly** — Графический конвейер OpenGL 3.3, GLSL-шейдеры, PCF Shadow Mapping, 4× MSAA HDR Bloom и Tone Mapping
  - **Daulet Ulgaisyn** — 15-костная иерархическая скелетная анимация, боевая физика 60 Гц, 8-Way Dash и Stencil Voronoi Split-Screen
  - **Alikhan Kokey** — Процедурные 3D-арены и лабиринты, трипланарные материалы, SDF-рендеринг текста, 3D Orbit Camera и Developer Mode
  - **Группа:** **IT-2502**

---

## Слайд 2 — Архитектура проекта, игровой цикл и распределение ролей (Section 2 & 3)

### Двухфазный игровой цикл (полноценная игра с циклом > 5 минут)
1. **Фаза 1 — `Hunt Mode` (3D-лабиринт):** Игрок исследует процедурно сгенерированный 3D-лабиринт (*Halls of Stone*, *Hedge Labyrinth*, *Neon Sewers*), ориентируясь по датчику **Death Sense** (пульсация сердца и направление следа), чтобы выследить 5 соперников.
2. **Фаза 2 — `Fight Mode` (3D-арена 60 Гц):** При столкновении с соперником запускается полноценный 3D-файтинг со Стендами (`Stand`) и Режимами (`Mode`), уникальными чертами персонажей (`Unique Traits`), рывками во все 8 сторон (`8-Way Omnidirectional Dash`), суператаками (`HHA` / `GHA`) и опасным окружением (открытый обрыв с летальной лавой на арене *Ember Forge*).

### Индивидуальный вклад участников команды (Section 2.3 & Section 4)

| Участник (IT-2502) | Модули и исходные файлы | Зона ответственности на защите |
|---|---|---|
| **Ali Yerzhanuly** | `src/gfx/renderer.*`, `src/gfx/shader.*`, `shaders/lit.*`, `depth.*`, `bright.frag`, `blur.frag`, `composite.frag` | Карта теней `2048×2048`, фильтрация `3×3 PCF`, освещение Key-Fill-Rim Blinn-Phong, 4× MSAA HDR Bloom и контур Depth-Sobel |
| **Daulet Ulgaisyn** | `src/game/rig.*`, `src/game/fight.*`, `src/game/characters.*`, `shaders/mask.frag` | Иерархические матрицы 15 суставов скелета, плавная интерполяция поз, 8-направленный дэш, динамический Voronoi Split-Screen через буфер трафарета (`GL_STENCIL_TEST`) |
| **Alikhan Kokey** | `src/game/arena.*`, `src/game/hunt.*`, `src/game/app*`, `shaders/ui.*`, `shaders/unlit.*` | Процедурная генерация 3D-арен и лабиринтов, трипланарные текстуры в шейдере, SDF-рендеринг шрифтов, 3D Orbit Camera и Developer Mode |

---

## Слайд 3 — Многопроходный графический конвейер OpenGL 3.3 Core (Section 4)

1. **Pass 1 — Shadow Depth Pass (`depth.vert` / `depth.frag`):** Рендерит непрозрачные объекты из ортографического пространства источника света в текстуру глубины `2048×2048` (`GL_DEPTH_COMPONENT24`).
2. **Pass 2 — 4× MSAA HDR Forward Lit & Unlit Pass (`lit.vert` / `lit.frag`, `unlit.vert` / `unlit.frag`):** Рендерит сцену в буфер `GL_RGBA16F` с 4× MSAA: освещение Key-Fill-Rim Blinn-Phong, мягкие тени `3×3 PCF`, до 8 динамических точечных источников света, процедурные трипланарные поверхности и атмосферный туман.
3. **Pass 3 — Dynamic Voronoi Split-Screen (`post.vert` / `mask.frag`):** При отдалении игроков в режиме 2P Versus заполняет буфер трафарета (`GL_STENCIL_TEST`) вдоль серединного перпендикуляра между бойцами и рендерит вторую камеру.
4. **Pass 4 — HDR Bright-Pass & Separable 9-Tap Gaussian Blur (`bright.frag` / `blur.frag`):** Разрешает 4× MSAA в текстуру `GL_RGBA16F`, выделяет яркие области ($L > 0.92$) и размывает их в два прохода (горизонтальный и вертикальный) в половинном разрешении.
5. **Pass 5 — Depth-Sobel Silhouette Ink Outline & Tone Mapping (`composite.frag`):** Линеаризует буфер глубины, находит силуэтные скачки глубины оператором Собеля, складывает с HDR Bloom и применяет мягкую компрессию светов (soft-knee tone mapping).
6. **Pass 6 — 2D SDF Font & HUD Overlay (`ui.vert` / `ui.frag`):** Рендерит интерфейс и сглаженный через `fwidth()` векторный текст.

---

## Слайд 4 — Математические уравнения в шейдерах (Section 4)

- **3×3 PCF и защита от Shadow Acne (`shaders/lit.frag`):**
  $$b = \max\Big(0.0018 \cdot (1 - \max(\mathbf{N}\cdot\mathbf{L}, 0)),\; 0.0004\Big)$$
  $$S_{\text{PCF}}(\mathbf{p}) = \frac{1}{9}\sum_{i=-1}^{1}\sum_{j=-1}^{1} \text{shadow2D}\!\left(\text{uShadowMap},\; (p_x + i\Delta_x,\, p_y + j\Delta_y,\, p_z - b)\right)$$
- **Key-Fill-Rim Blinn-Phong (`shaders/lit.frag`):**
  $$I_{\text{sun}} = S_{\text{PCF}} \cdot \Big(0.78 \max(\mathbf{N}\cdot\mathbf{L}, 0)\mathbf{C}_{\text{base}} + 0.22 \max(\mathbf{N}\cdot\mathbf{H}, 0)^{36}\Big) \odot \mathbf{C}_{\text{sun}}$$
  $$I_{\text{rim}} = \big(1 - \max(\mathbf{N}\cdot\mathbf{V}, 0)\big)^3 \cdot \big(0.25 + 0.35\max(N_y, 0)\big)\mathbf{C}_{\text{rim}}$$
- **Линеаризация глубины и силуэтный контур (`shaders/composite.frag`):**
  $$z_{\text{lin}}(d) = \frac{2 z_{\text{near}} z_{\text{far}}}{z_{\text{far}} + z_{\text{near}} - (2d - 1)(z_{\text{far}} - z_{\text{near}})}, \quad \text{edge} = \text{smoothstep}\!\left(0.035,\, 0.11,\, \frac{e}{z_0}\right)\big(1 - \text{smoothstep}(10,\, 22,\, z_0)\big)$$
- **Прямая кинематика 15-костного скелета (`src/game/rig.cpp`):**
  $$M_{\text{world}}(j) = M_{\text{world}}(\text{parent}(j)) \cdot T(\mathbf{p}_{\text{offset}}(j)) \cdot R_z(\psi_j) \cdot R_x(\theta_j) \cdot R_y(\phi_j)$$

---

## Слайд 5 — Производительность GPU и чек-лист (Section 4 & 8)

| Режим конвейера (`[F2]`) | FPS @ `1280×720` | FPS @ `1920×1080` |
|---|---|---|
| **Full Pipeline (4× MSAA + 2048 PCF + Bloom + Sobel)** | **168 FPS** (`5.95 мс`) | **142 FPS** (`7.04 мс`) |
| **No Bloom Pass** | **192 FPS** (`5.20 мс`) | **164 FPS** (`6.10 мс`) |
| **No Sobel Outline Pass** | **176 FPS** (`5.68 мс`) | **150 FPS** (`6.67 мс`) |
| **No 2048×2048 PCF Shadow Pass** | **215 FPS** (`4.65 мс`) | **185 FPS** (`5.40 мс`) |
| **Direct Unfiltered Pass** | **245 FPS** (`4.08 мс`) | **210 FPS** (`4.76 мс`) |
