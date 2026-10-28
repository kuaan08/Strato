# Auditoria de candidatos de shader e Vulkan para Strato Adreno

**Data:** 28 de outubro de 2026
**Tipo:** pesquisa, port seletivo e validação de CI; não é validação visual ou de desempenho no aparelho.

## Resumo executivo

Esta consolidação compara alterações selecionadas do Lemon e do Eden com o Strato e com as evidências locais declaradas. Foi integrado um port estreito do ajuste de arredondamento de `texture gather`, habilitado somente para drivers Vulkan Qualcomm Proprietary e Mesa Turnip. O CI do commit `f6922dc7` passou com testes, build arm64 e upload do APK [35]. Nove outras famílias permanecem adiadas até existir reprodução, medição ou lacuna de capacidade comprovada que justifique o risco; uma proposta específica — o fallback de índice nulo associado a uma etapa de conversão U8 no histórico do Eden — deve ser descartada como port, pois não preserva os índices e não corresponde ao caminho do Strato. “Adiar” não significa que o tema seja irrelevante: significa que não há base para aplicar agora um desvio correto e útil no Strato.

A seleção prioriza mudanças com possível relevância para Vulkan/Adreno, shaders, compatibilidade ou desempenho. Foram examinadas as instruções do projeto, a investigação anterior de artefatos duplicados, os trechos locais do perfil/emissores SPIR-V/ShaderManager/TraitManager e as auditorias independentes de Lemon v1.2.0/v1.2.2 e Eden #3334/#4212/#4565. A cobertura é de fontes públicas selecionadas; **não** é uma varredura exaustiva de todos os commits, branches, issues ou da matriz de dispositivos e drivers. PRs amplos contêm trabalhos não relacionados, e os resumos abaixo isolam apenas as famílias candidatas. [1] [2] [3] [4] [5]

O submódulo `app/libraries/shader-compiler` está fixado em `8717c8f6d7deab58032e042b027ed84838833824`. Não se deve decidir um cherry-pick ou port do compilador sem diff direto entre esse commit e a revisão upstream relevante, revisão do estado final (incluindo reverts/iterações) e adaptação às APIs do Strato. Os achados locais indicam que o Strato tem arquitetura e contratos de Vulkan próprios; equivalência com Lemon/Eden não pode ser inferida por semelhança de nomes de arquivo. [6]

### Disposição compacta

| ID | Família candidata | Disposição | Aplicabilidade observada no Strato |
|---|---|---|---|
| `lemon-gs-side-effects-adreno` | Deduplicar efeitos colaterais replicados em geometry shaders | **Adiar** | Ausente no compilador fixado; exige coordenação compiler/runtime/draw/descriptor. Nenhum geometry shader nas capturas KLIF fornecidas. |
| `adreno-turnip-gather-rounding` | Ajuste de coordenadas subpixel para texture gather | **Portado; CI aprovado; teste no aparelho pendente** | Profile/emissor agora aplicam o workaround somente para Qualcomm Proprietary e Mesa Turnip. Os shaders KLIF examinados não mostraram gather reconhecido, portanto isto não é causa demonstrada do ghosting desse jogo. |
| `lemon-fragment-translation-cache` | Cache de tradução SPIR-V de fragment shaders na sessão | **Adiar** | Não presente; APIs de environment/runtime diferem. Sem evidência de tradução lenta relevante no Strato. |
| `qcom-shader-feature-profile-gating` | Restringir recursos float-control/int64/WMEL para Qualcomm | **Adiar** | Política atual aceita capabilities consultadas; WMEL já está desabilitado no caminho fornecido. Gates exigem coerência de device, perfil e fallback. |
| `spirv-nonuniform-descriptor-capabilities` | Emissão SPIR-V de indexing não uniforme por tipo de recurso | **Adiar** | Lacuna do compilador parece concreta nos trechos, mas faltam provas de feature query/enable completas e shader reprodutor. Mudança geral Vulkan, não “otimização Adreno”. |
| `qcom-int64-lowering-fallback` | Emular operações int64 quando Qualcomm nativo for desabilitado | **Adiar** | Lowerer condicional existe; o pin está atrasado em relação ao upstream e falta ao menos `IAbs64` no esquema local. Sem shader Qualcomm falhando demonstrado. |
| `qcom-bgr5-image-view-swizzle` | Corrigir swizzle/formato B5G6R5 | **Adiar** | Não há flag BGR5 ou transform evidente nos trechos revistos; caminho completo de seleção de formato/mapping ainda precisa confirmação local. |
| `qcom-uint8-index-pass-fallback` | Fallback de índice U8 via buffer nulo no histórico do Eden | **Pular esse port** | Strato faz bind direto U8 condicionado pela capability, sem a etapa equivalente. O fallback nulo do commit intermediário/final não preserva os índices. Investigar separadamente compatibilidade U8. |
| `qcom-parallel-pipeline-compilation` | Serializar compilação paralela em drivers Qualcomm | **Adiar / já parcialmente presente** | Strato já seleciona pool de um worker para intervalo Qualcomm estreito; Eden usa outro predicado/versionamento. Não ampliar sem validar valores brutos do driver. |
| `android-msaa-resolve-path` | Cópia/resolve de imagens multisample no Android/Vulkan | **Adiar** | O caminho Strato é diferente; não há reprodução MSAA identificada nem equivalência de scheduler/render pass demonstrada. |
| `tiler-renderpass-attachment-load-store` | Reuso de anexos e LOAD/STORE/DONT_CARE para tilers | **Adiar** | Strato já deduplica views e promove alguns clears; não tem a arquitetura resolve-shadow/discard de Eden. Otimização depende de ciclo de vida completo. |

**Contagem:** port agora = 1; adiar = 9; pular = 1. O port gather foi aplicado como patch versionado para o pin `8717c8f6d7deab58032e042b027ed84838833824`; a aplicação e o teste host dedicado passaram localmente. O Actions `40626526568` também aprovou a suíte completa e gerou o APK arm64 de 28.529.582 bytes, SHA-256 `9e8ee25f224778ff9c2d5443fd58e337f9ab3545c38794c430b37f641055de96`. “Pular” aplica-se à proposta específica de usar buffer nulo para substituir conversão de índices, não à obrigação de verificar se o bind U8 direto é suportado em cada caminho real.

## Critério de seleção e evidência

Um candidato foi incluído quando as fontes upstream descrevem uma mudança material de shader/compiler, perfil de capability, driver Vulkan, renderização ou custo de compilação com possível pertinência ao Strato Adreno. A prioridade de port exige mais do que relato upstream: reprodução no Strato, identificação do driver Vulkan efetivamente ativo, compatibilidade com o pin/API do projeto, escopo estreito, testes determinísticos e plano de validação no dispositivo. Conforme as instruções do projeto, não se presume GPU/driver/capabilities pelo nome do telefone, não se alega ganho sem comparação da mesma cena/configuração e CI não substitui validação visual/desempenho em hardware. [1]

A evidência anterior de *Kill la Kill IF* limita duas hipóteses específicas: as duas capturas do pipeline cache não contêm estágio Geometry em seus registros; a busca no cache Adreno não encontrou os encodings de gather `TLD4`, `TLD4_b` e `TLD4S` reconhecidos pela tabela local. O decoder não reconheceu todas as words, logo isso não é prova de ausência de qualquer shader/variante. A captura problemática foi associada ao Turnip v26.0.0 R8-v1; suas notas alertam sobre falhas gráficas em emuladores Switch. Isso torna o driver selecionado uma hipótese mais forte para aquele ghosting do que os dois workarounds acima, mas não constitui A/B visual confirmado nem explica outras sessões ou jogos. [2] [3]

## Achados por família

### 1. Efeitos colaterais replicados em geometry shaders — `lemon-gs-side-effects-adreno` — **adiar**

Lemon v1.2.0 atribui spikes/flicker em *Marvel Ultimate Alliance 3* a efeitos colaterais repetidos pelas réplicas de geometry shader; a issue descreve contagens indiretas três vezes maiores antes da correção no ensaio reportado. A implementação final usa `atomicUMax` com geração por draw e stride, reclama um slot por primitiva e guarda os efeitos colaterais, mantendo a emissão normal de vértices. Não se deve copiar o pseudocódigo CAS inicial da issue: o mantenedor relata que a versão anterior podia permitir que uma invocação atrasada de draw antigo retomasse slots. A solução inclui buffer scratch sintético, geração push-constant, binding/descritor host e tratamento de instâncias e multidraw; indirect-count multidraw tem limitação de geração compartilhada. [4] [5]

No pin Strato não há flag específica, classificador IR, registro scratch nem guardas de geração; a integração também exigiria alocação/binding host e coordenação com pipeline layout e caminho de draw. É uma mudança de correção com risco de semântica, não uma otimização segura por associação a Adreno. A evidência de MUA3 é estreita (Adreno 830/Odin 3) e não valida outras gerações. Como KLIF não apresentou Geometry nas capturas disponíveis, não há nexo com o artefato investigado. Retomar apenas diante de reprodução Strato de shader GS com efeito colateral em driverID Qualcomm Proprietary ou Turnip identificado, após diff direto com Lemon v1.2.0; implementar limitações e testes de side effect/loop/instancing/multidraw explicitamente. [4] [5] [6]

### 2. Arredondamento de texture gather — `adreno-turnip-gather-rounding` — **portado; CI aprovado**

Lemon v1.2.2 relata seleção incorreta do bloco de texels em gathers junto a limites de textura (exemplo upstream: quadrados escuros nas sombras de TOTK). O emissor desloca x/y normalizados em `(1/512)/dimensão`, com consulta de tamanho em LOD 0, apenas em gather e Dref-gather e quando o perfil habilita o ajuste; outras operações de imagem permanecem inalteradas. Eden #4565 acrescenta Qualcomm Proprietary e Mesa Turnip à condição do perfil. Isso é correção semântica dirigida por driverID, não filtro específico de jogo nem ajuste universal de Adreno. [7] [8] [9]

O port Strato adiciona uma flag ao profile do compilador, ativa-a somente para `VK_DRIVER_ID_QUALCOMM_PROPRIETARY` e `VK_DRIVER_ID_MESA_TURNIP`, e desloca as coordenadas normalizadas em 1/512 de texel apenas nos emissores `ImageGather` e `ImageGatherDref`. Um patch versionado aplica essa mudança ao submódulo fixado e recusa pins diferentes; o workflow o aplica antes da compilação. O teste host confirma o gate por driverID, a propagação do profile, as dimensões tratadas e o escopo dos callsites; todos os testes host locais passaram. O Actions `40626526568` aprovou testes Android, regressões host, compilação C/C++ e `assembleMainlineRelease` no commit `f6922dc7` [35]. O APK arm64 `Strato-Adreno-Final-Gather-f6922dc7.apk` tem 28.529.582 bytes e SHA-256 `9e8ee25f224778ff9c2d5443fd58e337f9ab3545c38794c430b37f641055de96`; o conteúdo ZIP foi verificado. Ainda falta o A/B visual no aparelho. A auditoria do cache KLIF não encontrou gathers reconhecidos, então este port não explica nem promete corrigir a captura atual. [2] [7] [8] [9] [32] [33] [34]

### 3. Cache de tradução de fragment shaders — `lemon-fragment-translation-cache` — **adiar**

Lemon v1.2.0 introduz cache em memória de sessão de SPIR-V mais `Shader::Info`, com respostas às consultas de ambiente, runtime info, bindings e variantes incluídos na validação da chave; serializa misses concorrentes por hash e retém traduções acima de 30 ms. A release informa redução aproximada de 100 s para 70 s na carga de Splatoon 3 em um AYN Odin 3. É o único candidato desta lista primordialmente voltado a reduzir custo de tradução e não depende de driver Qualcomm específico. A cifra é relato de um workload/dispositivo, não benchmark replicado nem previsão de ganho no Strato. [10] [11]

O `ShaderManager` Strato traduz ao analisar etapas antes de criar runtime info/bindings no pipeline manager. O compilador pinado tem APIs/environment/runtime diferentes; `ReadTexturePixelFormat` no environment fornecido lança erro. Cache incorreto pode reutilizar SPIR-V ou metadados de outro binding, perfil, replacement ou estado de escala de resolução; também há questões de lifetime, locks e limite de memória. Cache guest e cache persistente de pipeline existentes não equivalem a essa camada. Adiar até profiling mostrar traduções de fragment repetidas e caras; então definir chave completa, política de memória e testes de equivalência cached/uncached antes de medir dispositivos. [6] [10] [11]

### 4. Gating de capabilities de shader para Qualcomm — `qcom-shader-feature-profile-gating` — **adiar**

Eden #4212 é uma manutenção Vulkan ampla e iterativa. No estado final relevante, sob driver Qualcomm Proprietary, desabilita float controls e recursos int64/atomic-int64; o gate de explicit workgroup layout (WMEL) é limitado a versões até `512.762.24`. A alteração final não desabilita `shaderFloat16`: isso ocorreu em uma tentativa intermediária e foi revertido. Os relatos de jogos no PR não isolam a causalidade destes gates. O tratamento depende de alterações correlatas de profile, lowering e falha/fallback de pipeline. [12] [13]

No material Strato, capabilities e propriedades float-control alimentam traits, `HostTranslateInfo` e `Shader::Profile`; WMEL já aparece hard-coded como falso. Alterar apenas a feature Vulkan pode deixar propriedades/profile incoerentes ou permitir emissão SPIR-V inválida. Desabilitar int64 exige cobertura das operações e tratamento determinístico de IR residual; não se deve incluir Turnip apenas por ser GPU Adreno ou por vendorID. Sem reprodução no driver alvo e sem completar testes/lowering, não há port seguro. [6] [12] [13]

### 5. Nonuniform descriptor indexing em SPIR-V — `spirv-nonuniform-descriptor-capabilities` — **adiar**

O commit Eden `eb9280...` acompanha quatro capacidades por tipo (sampled image, storage image, uniform texel buffer e storage texel buffer), rastreia IDs decorados e uso efetivo, emite NonUniform/capabilities somente quando necessário, e considera `SPV_EXT_descriptor_indexing` abaixo de SPIR-V 1.5. A especificação Vulkan requer consulta e habilitação das features opcionais pertinentes; promoção da extensão não torna cada bit universalmente disponível. É uma correção geral de compatibilidade SPIR-V, não um ajuste de desempenho ou de Adreno. [14] [15] [16]

O pin Strato não tem esses campos no profile; os trechos do emissor não decoram arrays sampled, rejeitam algumas formas de indexação dinâmica de imagem e não mostram tracking de capability. A lacuna merece acompanhamento, mas as fontes fornecidas não estabelecem se todo o encadeamento de query/enable Vulkan está implementado em outros pontos nem trazem shader que falhe. Port parcial do emissor ou flags presumidas poderiam gerar SPIR-V inválido. Reabrir após verificar feature chain completa e obter shader reprodutor; testar cada feature/per-resource-kind, versão SPIR-V e validação SPIRV-Tools antes de habilitar. [6] [14] [15] [16]

### 6. Lowering int64 sob desativação nativa — `qcom-int64-lowering-fallback` — **adiar**

O commit upstream adiciona casos específicos ao lowerer int64 existente, mas não implementa emulação universal de todas as operações de 64 bits. O gate que desabilita `shaderInt64` ocorre separadamente na política Qualcomm Proprietary da PR ampla. O pin Strato já invoca o lowerer quando `support_int64` é falso, mas sua tabela/opcodes é mais antiga: falta, por exemplo, `IAbs64` referenciado pelo patch upstream. Conversões que passam por F32 merecem atenção especial à precisão e casos limite. [12] [17] [18]

Nenhum shader ou falha do Strato fornecido prova necessidade de forçar esse caminho. Copiar o lowerer isoladamente não compila necessariamente; desativar capability sem cobrir operações residuais pode falhar em criação de pipeline ou mudar resultados. Adiar até reproduzir falha em shader e driver Qualcomm Proprietary específico, reconciliar esquema IR/frontend com pin, definir operações cobertas e validar semântica numérica, SPIR-V e comportamento de pipeline. Não generalizar para todo Adreno nem Turnip. [6] [17] [18]

### 7. Swizzle B5G6R5 — `qcom-bgr5-image-view-swizzle` — **adiar**

Eden #3334 remove um flag antigo que forçava emulação BGR565 e o swap R/B correspondente em views B5G6R5 não render-target. A alteração é uma remoção de transformação duplicada, não introdução de uma nova emulação. O PR também mexe em funcionalidades sem relação; sua motivação BGR5 é vinculada a Qualcomm stock/system Vulkan, mas não fornece reprodução isolada do swizzle nem um gate BGR5 por versão. A presença de `VK_KHR_maintenance5` também não prova suporte a B5G6R5: a extensão adiciona outros formatos. [19] [20] [21]

Os trechos Strato revistos mostram formato Vulkan B5G6R5 e mapping default identity, sem flag ou transformação B5G6R5 no emissor SPIR-V. Ainda falta verificar no repositório o caminho completo de fallback de formato e origem dos component mappings. Modificar um lado sem rastrear seleção de formato e view pode inverter canais de forma ampla. Adiar até captura reproduzível em driver Qualcomm Proprietary identificado e rastreamento end-to-end da view; se confirmado, isolar correção de mapping e testar pixels assimétricos, fallback R5G6B5, render targets e outros formatos compactados. [6] [19] [20] [21]

### 8. Fallback de índice U8 por buffer nulo — `qcom-uint8-index-pass-fallback` — **pular esse port**

O histórico contém commit intermediário que evitava criar `Uint8Pass` em Qualcomm Proprietary e substituía índice de 8 bits por bind de buffer nulo com tipo UINT16. O estado final de Eden não conserva aquele gate amplo do construtor; o fallback nulo permanece para determinada combinação sem suporte/sem pass, mas não converte nem preserva os índices. Portanto não é correção semântica para copiar. [22] [23] [24]

Strato não tem essa etapa compute nos trechos auditados: detecta suporte e `IndexBufferState` faz bind direto `eUint8EXT`, sem converter quando a feature não está disponível. Isso levanta uma verificação independente da elegibilidade real do comando/feature, não justifica descartar índices. **Pular** o fallback nulo. Se surgir falha reproduzível, projetar conversão U8→U16 correta, preservando restart `0xFF`→`0xFFFF`, offset, `firstIndex`, ranges e lifetime, ou rejeitar explicitamente caminho incompatível. Confirmar se o bind direto pode ocorrer em dispositivo que não habilitou a feature. [6] [22] [23] [24]

### 9. Compilação paralela de pipelines — `qcom-parallel-pipeline-compilation` — **adiar; comportamento parcial já existe**

Eden adiciona quirk para Qualcomm Proprietary com `driverVersion` normalizada e limite `< 512.615.512`; não é alteração de shader nem ajuste para Turnip. Strato já marca compilação paralela problemática em intervalo raw estreito, `VK_MAKE_VERSION(512,615,0)`–`VK_MAKE_VERSION(512,615,512)`, e configura pool com um worker. A divergência é no predicado/interpretação de versionamento, não ausência total da funcionalidade. Vulkan define `driverVersion` como implementação-específica, então comparar números apresentados em formatos diferentes sem valores crus de aparelho não comprova equivalência. Serializar pode melhorar estabilidade, mas reduz concorrência e pode elevar latência. [25] [26] [27]

Adiar ampliação até registrar `driverID` e raw `driverVersion` no(s) dispositivo(s) afetado(s) e testar ambos os predicados em fronteiras e versões atingidas/não atingidas. Não alterar shader compiler nem estender para Turnip sem evidência própria. [25] [26] [27]

### 10. MSAA resolve/cópia Android/Vulkan — `android-msaa-resolve-path` — **adiar**

A família MSAA da PR #4212 não é uma edição isolada do recompiler: envolve shaders host, blits/resolve de cor, caminho Android, depth/stencil, render passes, imagens e sincronização; o diff rejeita explicitamente stencil-only resolve em um caminho. Os requisitos de storage-image multisample e depth/stencil resolve são capabilities distintas. Relatos de ganhos/correções em títulos e dispositivos no PR não quantificam uma melhoria universal, nem demonstram comportamento do Strato ou Turnip. [12] [28] [29]

Strato tem outro arranjo de `Texture`, `CommandScheduler` e `GraphicsPipelineAssembler`; as fontes dadas não mostram reprodução de transfer/resolve multisample ou que a capability esteja ausente em todo caminho. Um port parcial pode perder depth/stencil, quebrar layout/barreiras ou produzir dados incorretos. Adiar até comprovar imagem multisample e operação reproduzível, depois desenhar solução nativa com testes de pixel, depth/stencil, capability branches e validação de sincronização nos drivers específicos. [6] [28] [29]

### 11. LOAD/STORE e DONT_CARE em tilers — `tiler-renderpass-attachment-load-store` — **adiar**

Eden combina escolhas de CLEAR/LOAD e STORE/DONT_CARE com attachments resolve/shadow, discard masks, scheduler e ciclo de vida de imagem. DONT_CARE não é troca segura de estado: conteúdos anteriores deixam de ser confiáveis e STORE DONT_CARE pode descartar o que consumidores posteriores precisam. A estratégia upstream inclui Qualcomm, ARM, Samsung e Turnip por driverID para política de tiler, não todo Adreno por marca. Os relatos de performance são qualitativos, sem protocolo/medição reprodutível no Strato. [12] [30] [31]

Strato já deduplica views de attachments dentro de render pass e tem promoção de alguns full-clears, mas não a arquitetura Eden de resolve-shadow/discard nos callsites examinados. A correção depende de invalidar/reconstruir sombras após writes, copies, uploads, reinterpret e leituras; o pin do shader compiler não torna essa alteração intercambiável. Adiar até profiling indicar custo e arquitetura/provas Strato demonstrarem todos os consumidores e invalidações; validar cache keys, partial clears, subpasses, MRT, MSAA, depth/stencil, outputs e sincronização. [6] [30] [31]

## Riscos transversais e plano mínimo para reconsideração

1. **Identidade do driver:** registrar no dispositivo `driverID`, `driverVersion` bruto, versão Vulkan, GPU e caminho efetivamente carregado; manter Qualcomm Proprietary separado de Mesa Turnip. Não inferir o caminho pelo nome comercial do telefone/GPU. [1] [25] [26]
2. **Pin e estado upstream:** antes de portar, diffar diretamente o pin `8717c8...` contra os arquivos e commit final upstream; descartar pseudocódigo/commits intermediários revertidos e alterações não relacionadas em PRs grandes. Revisar dependências de frontend/IR, profile e integração host.
3. **Reprodução causal:** identificar jogo, versão, cena, shader/opcodes/stages e resultado esperado. Uma correção inspirada em MUA3, TOTK ou outra plataforma não prova ligação com KLIF nem com outro jogo Strato.
4. **Testes de corretude:** primeiro testes host determinísticos para lógica pura, compilação/validação SPIR-V e casos negativos; depois integração Vulkan com output conhecido, validação de features, lifetime e sincronização. Em mudanças de descritor/render pass/índice, testar binding, cache e caminhos com feature ausente.
5. **Alegações de ganho:** só após A/B repetível no mesmo aparelho, driver, APK/commit, jogo/cena/configuração e caches; registrar frametime/FPS, temperatura aproximada, duração, artefatos e crashes. Relatos upstream são motivação, não medição no Poco. O port atual compilou no CI e passou os testes automatizados, mas nenhum jogo foi executado nem houve medição visual ou de desempenho no aparelho.

## Referências

[1]: https://github.com/kuaan08/Strato/blob/main/CLAUDE_PROJECT_INSTRUCTIONS.md "Instruções do projeto Strato Adreno — escopo, validação e medições"
[2]: https://github.com/kuaan08/Strato/blob/main/docs/ADRENO_DUPLICATED_GRAPHICS_RESEARCH.md "Investigação anterior de artefatos gráficos duplicados e caches de KLIF"
[3]: https://github.com/K11MCH1/AdrenoToolsDrivers/releases/tag/v26.0.0-rc08 "Turnip v26.0.0 R8 — notas da release"
[4]: https://github.com/Ghael-V/Lemon-Project/releases/tag/v1.2.0 "Lemon Project v1.2.0 — correção de efeitos colaterais em geometry shaders e cache de tradução"
[5]: https://github.com/Ghael-V/Lemon-Project/issues/7 "Lemon issue #7 — MUA3, replicação de efeitos colaterais e evolução para atomicUMax"
[6]: https://github.com/strato-emu/shader-compiler/tree/8717c8f6d7deab58032e042b027ed84838833824 "Strato shader-compiler — commit pinado 8717c8f6d7deab58032e042b027ed84838833824"
[7]: https://github.com/Ghael-V/Lemon-Project/releases/tag/v1.2.2 "Lemon Project v1.2.2 — correção de texture gather"
[8]: https://git.eden-emu.dev/eden-emu/eden/pulls/4565 "Eden PR #4565 — habilitação do ajuste de gather para Qualcomm e Turnip"
[9]: https://github.com/Ghael-V/Lemon-Project/blob/v1.2.2/src/shader_recompiler/backend/spirv/emit_spirv_image.cpp "Lemon v1.2.2 — emissor SPIR-V de gather e compensação subpixel"
[10]: https://github.com/Ghael-V/Lemon-Project/blob/v1.2.0/src/video_core/renderer_vulkan/vk_fragment_translation_cache.h "Lemon v1.2.0 — contrato do cache de tradução de fragment shaders"
[11]: https://github.com/Ghael-V/Lemon-Project/blob/v1.2.0/src/video_core/renderer_vulkan/vk_fragment_translation_cache.cpp "Lemon v1.2.0 — gravação de consultas, chave de variantes e sincronização do cache"
[12]: https://git.eden-emu.dev/eden-emu/eden/pulls/4212 "Eden PR #4212 — manutenção Vulkan ampla, recursos Qualcomm e caminhos MSAA"
[13]: https://git.eden-emu.dev/eden-emu/eden/commit/28a72dea93372714bf327d8c00b9504cd290c28b "Eden — estado final das gates Qualcomm de float controls, int64 e WMEL"
[14]: https://git.eden-emu.dev/eden-emu/eden/commit/eb9280dedfb5e49e17a0bb586c2be87c4b769625 "Eden — tracking de nonuniform descriptor indexing e capabilities SPIR-V"
[15]: https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceDescriptorIndexingFeatures.html "Khronos Vulkan — features opcionais de descriptor indexing por tipo de recurso"
[16]: https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_descriptor_indexing.html "Khronos SPIR-V — extensão EXT_descriptor_indexing e decorações NonUniform"
[17]: https://git.eden-emu.dev/eden-emu/eden/commit/5e6f3730b31e6c9b5ddf06ef8f4d07daad95ff0d "Eden — adições específicas ao lowerer int64→int32"
[18]: https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceFeatures.html "Khronos Vulkan — requisito shaderInt64 para tipos inteiros de 64 bits"
[19]: https://git.eden-emu.dev/eden-emu/eden/pulls/3334 "Eden PR #3334 — alterações Vulkan para Qualcomm e contexto BGR565"
[20]: https://git.eden-emu.dev/eden-emu/eden/commit/1fbace438c375ca1e33f3cb09d7d85249cb51293 "Eden — remoção da transformação duplicada B5G6R5 e alterações relacionadas"
[21]: https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_maintenance5.html "Khronos Vulkan — formatos introduzidos por VK_KHR_maintenance5"
[22]: https://git.eden-emu.dev/eden-emu/eden/commit/85843edc0deb1579580b5333419edfd8e795e918 "Eden — commit intermediário de fallback Uint8Pass específico a Qualcomm"
[23]: https://git.eden-emu.dev/eden-emu/eden/src/commit/1fbace438c375ca1e33f3cb09d7d85249cb51293/src/video_core/renderer_vulkan/vk_buffer_cache.cpp "Eden final — bind de índice e estado do caminho Uint8Pass"
[24]: https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_index_type_uint8.html "Khronos Vulkan — extensão de tipo de índice de 8 bits"
[25]: https://git.eden-emu.dev/eden-emu/eden/src/commit/1fbace438c375ca1e33f3cb09d7d85249cb51293/src/video_core/vulkan_common/vulkan_device.cpp "Eden — predicate de versão para compilação paralela Qualcomm"
[26]: https://github.com/kuaan08/Strato/blob/main/app/src/main/cpp/skyline/gpu/trait_manager.cpp "Strato — quirk Qualcomm para compilação paralela de pipelines"
[27]: https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceProperties.html "Khronos Vulkan — driverVersion é definido pela implementação"
[28]: https://git.eden-emu.dev/eden-emu/eden/commit/eb9280dedfb5e49e17a0bb586c2be87c4b769625.diff "Eden #4212 — diff de resolve MSAA, shaders host e render pass"
[29]: https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_depth_stencil_resolve.html "Khronos Vulkan — regras e modos de resolve depth/stencil"
[30]: https://git.eden-emu.dev/eden-emu/eden/src/commit/eb9280dedfb5e49e17a0bb586c2be87c4b769625/src/video_core/renderer_vulkan/vk_render_pass_cache.cpp "Eden — variants de render pass, CLEAR/LOAD, STORE/DONT_CARE e resolve"
[31]: https://docs.vulkan.org/spec/latest/chapters/renderpass.html "Khronos Vulkan — semântica de conteúdo e operações de load/store em render pass"
[32]: https://github.com/kuaan08/Strato/blob/main/patches/shader-compiler/0001-adreno-gather-rounding.patch "Patch versionado para o submódulo shader-compiler"
[33]: https://github.com/kuaan08/Strato/blob/main/scripts/apply_shader_compiler_patches.sh "Aplicador de patch com verificação do pin do submódulo"
[34]: https://github.com/kuaan08/Strato/blob/main/tests/test_adreno_gather_workaround.py "Regressão host do gate e do escopo do workaround gather"
[35]: https://github.com/kuaan08/Strato/actions/runs/40626526568 "GitHub Actions final do commit f6922dc7"
