# Revisão técnica: Strato Adreno, Poco X5 Pro e Lemon Project

**Escopo.** Revisei a branch `main` do Strato e o espelho GitHub do Lemon Project. A branch `testonly` foi ignorada conforme solicitado. O fabricante especifica o Poco X5 Pro 5G com Snapdragon 778G e GPU Adreno 642L [1]. Isso identifica o alvo de hardware, mas não informa a versão de Android, driver Vulkan instalado ou comportamento real do seu aparelho.

## O que encontrei

O Lemon é um emulador Android derivado do core comunitário do yuzu. Seu README informa que o GitHub é um espelho do repositório principal [2]. A arquitetura é diferente da do Strato: o Lemon organiza a renderização em `video_core`, enquanto o Strato mantém o caminho de GPU na árvore `skyline/gpu`. A semelhança de ambos usarem Vulkan não torna seus patches intercambiáveis.

Duas alterações do Lemon merecem ser tratadas como ideias para investigação, não como código para copiar. A v1.2.2 habilita um ajuste de tradução de shaders para drivers Qualcomm e Mesa Turnip para corrigir sombras específicas em TOTK; a nota descreve correção visual, não ganho de FPS [3]. A v1.2.0 relata um cache de tradução que reduziu o tempo de carregamento de Splatoon 3 em um AYN Odin 3. Esse resultado é específico àquele aparelho, jogo e implementação; não prevê o desempenho no Poco [4]. O código relacionado depende das abstrações de tradução e Vulkan do Lemon [5] [6].

No Strato, já existem traits e caminhos por driver, seleção de Vulkan, opções para driver customizado, validação da importação de memória AdrenoTools e regras próprias para formatos/aliasing [7]. Também há cache de shaders/pipelines, staging de memória, conversões de texturas e escala de resolução. Esses caminhos indicam onde investigar, mas a leitura do código não mostra qual deles limita um jogo no Poco.

## Alteração aplicada ao scheduler

A revisão encontrou duas expressões no scheduler que não correspondiam à semântica esperada de uma média ponderada da duração do timeslice e de uma estimativa restante. A atualização usava o timestamp absoluto dentro do termo ponderado; a estimativa com histórico limitava o resultado a no máximo um tick. O scheduler utiliza esses valores para estimar o tempo até uma thread em espera poder rodar [8].

Extraí o cálculo para helpers puros em `app/src/main/cpp/skyline/kernel/scheduler_timing.h`. A média agora pondera 25% do valor anterior e 75% da duração observada, sem multiplicação que possa transbordar `uint64_t`. A estimativa restante usa a diferença entre a média e o tempo decorrido, com piso de um tick depois que essa média se esgota. O comportamento sem histórico foi preservado.

Adicionei `tests/scheduler_timing_test.cpp` e `tests/test_scheduler_timing.py`, cobrindo EWMA, início sem histórico, saturação e limites de `uint64_t` com ASan/UBSan. Também acrescentei à CI os testes host de scheduler e frame pacing. O teste existente de frame pacing cobre intervalos e jitter em relógios de host diferentes [9]. Os dois testes passaram localmente, assim como os testes host de resolução, memória, diagnósticos e branding executados nesta sessão.

Essa correção torna a estimativa do scheduler coerente e testável. Não foi medido FPS no aparelho, portanto não afirmo que ela aumente desempenho em jogos. O commit `f8433793` passou nos testes host, testes unitários Android e compilação Release de arm64; o Actions publicou o artefato `Strato-Adreno-APK` na [execução 40536567344](https://github.com/kuaan08/Strato/actions/runs/40536567344) [13]. Isso valida o build e os testes automatizados, não o desempenho ou a compatibilidade no Poco.

## Primeiro teste manual informado

O usuário relatou que testou o APK, que o aplicativo abre e que os jogos aparentemente iniciam/funcionam. Isso é um smoke test inicial promissor. Não foram informados o aparelho exato usado, títulos e cenas testados, driver Vulkan, duração, configurações, FPS/frametime, temperatura ou presença de artefatos; portanto, não deve ser interpretado como benchmark nem como validação ampla de compatibilidade.

## Próximos passos seguros

1. Usar o APK produzido pela CI para identificar no próprio aparelho a versão de Android e o driver Vulkan efetivamente ativo. Guardar essas informações junto com o SHA do APK e uma cena/save repetível.
2. Fazer uma linha de base no Poco: mesma cena, configurações, taxa de atualização e condição térmica; registrar frametime p50/p95, FPS sustentado, temperatura, falhas e artefatos. Repetir antes de comparar qualquer alteração.
3. Investigar primeiro regressões reproduzíveis ou correções de imagem. Para uma hipótese de shader gather do Lemon, reproduzir o defeito específico e confirmar que o tradutor do Strato tem um ponto de integração equivalente antes de modificar o perfil do driver.
4. Só testar importação direta de memória, escala interna, caches ou opções de compilação em A/B isolados. Manter os padrões atuais como controle e verificar qualidade visual, estabilidade e memória, além de FPS.
5. Não fazer cherry-pick de código do Lemon sem adaptar às abstrações do Strato e revisar licença, créditos e avisos do arquivo. O Lemon declara GPL-3.0-or-later, mas arquivos individuais podem trazer avisos próprios; o Strato também contém licenças e avisos por componente [2] [11] [12].

## O que não alterei

Não forcei clocks, resolução ou afinidade de CPU. Não mudei heurísticas de aliasing Adreno, flags Vulkan, importação KGSL, sincronização, fences, cache de pipeline ou formato de textura. Não substituí NCE por Dynarmic e não copiei código do Lemon. Essas mudanças precisam de uma falha reproduzível, equivalência técnica e validação real no driver Adreno 642L. As recomendações da Qualcomm também alertam que certos usos de mutable-format podem afetar desempenho e UBWC; não é seguro generalizar uma regra de outro emulador [10].

## Referências

[1]: https://www.mi.com/global/product/poco-x5-pro-5g/specs/ "Especificações oficiais do POCO X5 Pro 5G"
[2]: https://github.com/Ghael-V/Lemon-Project/blob/25adc8800050828deb6f14123f6724bf92feec4b/README.md "Lemon Project README no commit revisado"
[3]: https://github.com/Ghael-V/Lemon-Project/releases/tag/v1.2.2 "Lemon Project v1.2.2"
[4]: https://github.com/Ghael-V/Lemon-Project/releases/tag/v1.2.0 "Lemon Project v1.2.0"
[5]: https://github.com/Ghael-V/Lemon-Project/blob/25adc8800050828deb6f14123f6724bf92feec4b/src/video_core/renderer_vulkan/vk_pipeline_cache.cpp#L440-L450 "Perfil Vulkan do Lemon para gather"
[6]: https://github.com/Ghael-V/Lemon-Project/blob/25adc8800050828deb6f14123f6724bf92feec4b/src/video_core/renderer_vulkan/vk_fragment_translation_cache.h "Cache de tradução de fragment shaders do Lemon"
[7]: https://github.com/kuaan08/Strato/blob/main/app/src/main/cpp/skyline/gpu.cpp "Strato: inicialização e seleção Vulkan"
[8]: https://github.com/kuaan08/Strato/blob/main/app/src/main/cpp/skyline/kernel/scheduler.cpp "Strato: scheduler"
[9]: https://github.com/kuaan08/Strato/blob/main/tests/test_frame_pacing.py "Strato: teste host de frame pacing"
[10]: https://docs.qualcomm.com/bundle/publicresource/topics/80-78185-2/mobile_best_practices.html "Qualcomm Adreno GPU on Mobile: Best Practices"
[11]: https://github.com/kuaan08/Strato/blob/main/LICENSE.md "Licença principal do Strato"
[12]: https://github.com/Ghael-V/Lemon-Project/blob/25adc8800050828deb6f14123f6724bf92feec4b/LICENSE.txt "Licença principal do Lemon Project"
[13]: https://github.com/kuaan08/Strato/actions/runs/40536567344 "GitHub Actions: build de arm64 e testes do commit f8433793"
