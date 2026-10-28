# Instruções do projeto — Strato Adreno

Você está ajudando a desenvolver o repositório **Strato Adreno**, uma variante de desenvolvimento baseada no Strato (que continua o trabalho do Skyline). O repositório é https://github.com/kuaan08/Strato e a branch principal é `main`.

## Objetivo

Preparar e melhorar esta variante para GPUs Adreno em Android, começando pela validação no **Poco X5 Pro 5G**. Trate o aparelho como alvo de testes, não como prova de que o emulador já está otimizado ou totalmente compatível. O código ainda precisa ser medido no dispositivo real antes de declarar ganhos de desempenho.

## Estado atual

- O projeto já compila pelo GitHub Actions. O workflow `.github/workflows/build.yml` executa testes host de branding, descoberta de driver Vulkan, tradução pt-BR, indicador de abas, tema/movimento da biblioteca, validação/reclaim de memória, frame pacing, scheduler, envelope do cache Vulkan e escala de resolução; também executa testes unitários Android e compila `assembleMainlineRelease` em pushes e pull requests para `main`, em branches `release/**` e sob execução manual.
- O APK fica no artefato `Strato-Adreno-APK` da execução do Actions. É assinado com a chave de depuração do Gradle e serve para testes; não é uma build de distribuição assinada.
- O `applicationId` atual é `org.stratoemu.stratoadreno`. O namespace e os pacotes Kotlin/C++ continuam `org.stratoemu.strato`/Strato por compatibilidade interna. Não os renomeie sem necessidade demonstrada.
- O identificador de pacote é diferente da antiga variante Mali. Instalação paralela é possível, mas os dados e saves internos não são migrados automaticamente entre pacotes.
- A integração automática que baixava e instalava PanVK para Mali foi removida desta variante. O app deve usar o driver Vulkan do sistema por padrão. Não inclua drivers customizados no APK sem uma decisão explícita, licença compatível, origem verificável e testes.
- O suporte genérico existente a Vulkan, Adreno/KGSL, seleção de driver compatível e recursos como importação direta de memória já está no código. Primeiro entenda as condições existentes antes de propor alterações.
- Parte de nomes de arquivos, recursos visuais e estilos herdados ainda pode conter referências históricas a Mali. Preserve os créditos e avisos legais; não faça substituições globais cegas.
- A compilação completa e os testes de CI foram confirmados no commit `0c540070` (execução Actions [40604775512](https://github.com/kuaan08/Strato/actions/runs/40604775512)), incluindo o build Android arm64 e o artefato APK. Isso confirma build/testes de CI, não compatibilidade ou desempenho no Poco.
- O scheduler usa helpers puros em `app/src/main/cpp/skyline/kernel/scheduler_timing.h` para a EWMA da duração real e a estimativa saturada de timeslice restante; `tests/test_scheduler_timing.py` valida os limites com ASan/UBSan. A correção não prova, por si só, ganho de desempenho em jogos.
- O wake de threads estacionadas usa uma decisão pura em `app/src/main/cpp/skyline/kernel/scheduler_wake.h`, com o caso de fila sem próximo thread coberto em `tests/test_scheduler_wake.py`. A importação do cache Vulkan valida cabeçalho/tamanho antes da alocação; `tests/test_pipeline_cache_file.py` cobre limites e truncamento. A JNI de metadados do driver retorna o fallback nullable Kotlin quando o loader/símbolo/dispositivo falha; `tests/test_gpu_driver_discovery.py` protege essas guardas. `SvcUnmapMemory` valida origem emprestada e destino de stack antes de alterar a tabela; `tests/test_memory_attributes.py` cobre o ciclo válido e rejeita unmap repetido ou origem não mapeada.
- A interface inclui português brasileiro em `app/src/main/res/values-b+pt+BR/`, registrado em `locales_config.xml` e no seletor de idioma do app. O teste `tests/test_ptbr_resources.py` confere cobertura das strings, placeholders, arrays e registro do locale; textos de licenças e mensagens internas de log podem continuar no idioma original.
- As paletas Strato Blue, Emerald, Violet e AMOLED (modo escuro) são escolhidas pela preferência `app_color_palette`. Material You tem prioridade quando ativo; as cores de fundo e superfície devem vir do tema claro/escuro e não ser hardcoded.
- A biblioteca de jogos agora usa atributos Material3 para fundo, busca, dock, botões, cards e seleção; o título visível vem de `@string/app_name` (Strato Adreno). `tests/test_library_theme_motion.py` protege essa integração, os ícones do sistema e as animações.
- A interface usa animações curtas e pontuais: entrada/remoção da lista de jogos, abertura do painel do jogo selecionado, feedback do dock e transições entre categorias das configurações. Não adicione animações contínuas nem trabalho visual à Activity de emulação.

## Compilação e verificações

O projeto usa Gradle 8.2, Java 17, Android SDK `compileSdk 34`, NDK `26.1.10909125` e CMake 3.22.1. Clone sempre com os submódulos:

```bash
git clone --recursive https://github.com/kuaan08/Strato.git
cd Strato
./gradlew assembleMainlineRelease
```

Para rodar os testes de unidade usados pelo CI:

```bash
./gradlew testMainlineReleaseUnitTest
```

O workflow também executa `python3 tests/test_adreno_branding.py`, `python3 tests/test_gpu_driver_discovery.py`, `python3 tests/test_ptbr_resources.py`, `python3 tests/test_settings_tab_indicator.py`, `python3 tests/test_library_theme_motion.py`, `python3 tests/test_mali_ui_consistency.py`, `python3 tests/test_memory_attributes.py`, `python3 tests/test_memory_reclaim.py`, `python3 tests/test_frame_pacing.py`, `python3 tests/test_scheduler_timing.py`, `python3 tests/test_scheduler_wake.py`, `python3 tests/test_pipeline_cache_file.py` e `python3 tests/test_resolution_scaling.py`. Faça alterações pequenas, rode os testes relevantes e confira `git diff --check`. Em alterações nativas, garanta que o workflow inteiro — incluindo compilação C/C++ via CMake — continue passando.

## Regras para otimização no Poco X5 Pro

1. Não presuma o modelo exato da GPU, versão do driver, recursos Vulkan ou comportamento térmico só pelo nome comercial do telefone. Sempre que possível, peça ou leia do aparelho as informações reais: modelo/codename, Android, driver Vulkan, GPU e logs do emulador.
2. Não adicione desvios condicionais baseados no nome do Poco nem hardcode de IDs de GPU sem necessidade comprovada. Prefira a detecção existente por capabilities e identificadores Vulkan.
3. Não altere por tentativa os caminhos de memória KGSL, importação direta, sincronização, fences, aliasing de formatos, tiling/compressão, compilação de shaders, cache de pipelines, filas ou extensões Vulkan. Mudanças nesses pontos podem causar corrupção gráfica, crashes, consumo elevado ou incompatibilidade com outros drivers Adreno.
4. Não force clocks, não desative limites térmicos e não aumente a resolução interna padrão com base em suposições. Mudanças desse tipo podem elevar temperatura, consumo e instabilidade.
5. Antes de alegar ganho, obtenha uma linha de base e compare a mesma cena/jogo/configuração. Registre versão do APK/commit, Android e driver, jogo e cena, configurações, FPS e frametime, temperatura aproximada, duração do teste, artefatos e crashes. Repita o teste para separar ganho real de variação.
6. Priorize primeiro correções reproduzíveis, estabilidade, logging diagnóstico de baixo custo e testes automatizados. Mantenha otimizações experimentais isoladas e fáceis de reverter; não mude padrões globais sem evidência.

## Licenças e integridade

Preserve os avisos de copyright, créditos do Strato/Skyline e as licenças de cada submódulo. O código-base Strato está sob GPL-3.0-or-later, mas submódulos e componentes de terceiros podem ter termos próprios. Não copie assets, drivers ou código sem confirmar a licença e a procedência. Não inclua ROMs, jogos, firmware ou chaves no repositório, artefatos ou logs.

## Como trabalhar

Antes de editar, inspecione a implementação e os testes relacionados e explique resumidamente o risco da mudança. Não reescreva subsistemas grandes nem faça renomeações em massa por estética. Mantenha mudanças pequenas, explique os arquivos alterados e os comandos de teste executados. Se não houver aparelho disponível, declare explicitamente que a mudança foi validada apenas por build/testes em CI, e não por medição no Poco.
