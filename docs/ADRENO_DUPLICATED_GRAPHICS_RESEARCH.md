# Investigação de artefatos duplicados no Strato Adreno

**Data:** 27 de outubro de 2026

**Escopo:** captura de *Kill la Kill IF*, log que a acompanhou, ZIPs `s.zip` e `to.zip`, comparação com Lemon e outros emuladores de Switch. Nenhum APK intermediário foi criado; o APK final foi produzido uma vez pelo Actions no commit `34d36d2b` [9].

## Captura original e driver selecionado

A imagem `32289.png` foi enviada junto com um `emulation.log` separado, e não está dentro dos ZIPs. Esse log confirma que a sessão da imagem carregou o Mesa Turnip v26.0.0 R8-v1 pelo arquivo `vulkan.ad07xx.so`. O driver enumera o dispositivo como “Adreno 7c+ Gen 3”; o POCO X5 Pro 5G usa Snapdragon 778G com Adreno 642L segundo a Xiaomi [7]. Como o nome veio do driver customizado, ele não prova uma troca de hardware, mas reforça que a biblioteca selecionada não corresponde ao nome normal da GPU do aparelho.

As notas oficiais do pacote Turnip v26.0.0 R8 avisam que emuladores de Switch podem iniciar e ainda assim renderizar gráficos muito defeituosos [8]. Isso se ajusta muito melhor ao ghosting amplo da captura do que as hipóteses de geometry shader ou texture gather, para as quais os caches do jogo não mostram o estágio/opcode esperado. É a explicação mais forte disponível, mas ainda não é um A/B visual confirmado: o ZIP `s.zip` é uma sessão separada, com o driver Qualcomm do sistema, e não contém a mesma cena.

O Strato agora evita carregar especificamente Turnip v26.0.0 R8 para emulação Switch e usa o driver Vulkan do sistema; a tela de drivers também mostra um aviso em pt-BR e troca para o sistema se essa versão estiver selecionada. A proteção é limitada a essa release conhecida, não a todos os drivers Turnip.

## O que os caches dos ZIPs permitem concluir

O `s.zip` registra a sessão de *Kill la Kill The Game - IF* no pacote `org.stratoemu.stratoadreno`, Adreno 642L, driver Vulkan proprietário Qualcomm do sistema e escala 1x. O arquivo `graphics_pipeline_cache/0100A3F00C536000` tem cabeçalho `PCHE`, versão 3, e 87 registros de bundle lidos estruturalmente conforme o formato implementado em `PipelineCacheManager` e `PipelineStateBundle` no Strato.

Os seis índices de estágio do bundle são `VertexCullBeforeFetch=0`, `Vertex=1`, `TessellationInit=2`, `Tessellation=3`, `Geometry=4` e `Pixel=5` (`maxwell/types.h`). A leitura dos 87 registros encontrou bytecode não vazio somente nos índices 1 e 5. O índice 4 (Geometry) estava vazio em todos.

Também parsei `graphics_pipeline_cache/0100A3F00C536000.staging` de `to.zip`, a captura antiga de Strato Mali para o mesmo título. Os 47 registros completos registram bytecode apenas nos índices 1 e 5; Geometry também está vazio nos 47. Isso não prova que nenhuma outra cena ou sessão use geometry shaders, mas duas capturas distintas da mesma title ID não dão suporte à hipótese de que um geometry shader causou a duplicação mostrada.

A captura Adreno tem 87 binários de Vertex e 87 de Pixel, mas somente 12 binários Vertex únicos e 22 Pixel únicos por comparação integral. Os arquivos de shader serializados somam 94.992 bytes, dos quais 27.832 pertencem aos 34 binários únicos. Isso mostra repetição entre pipelines, não custo de tradução: o log mede 246 ms para carregar os pipelines, mas não separa o tempo de parsing/tradução.

## Inspeção estática dos shaders guest

Analisei os 174 binários recuperados do cache Adreno com o cabeçalho e as tabelas de instruções Maxwell do shader compiler fixado pelo Strato. Os três encodings de gather reconhecidos pelo decoder local (`TLD4`, `TLD4_b` e `TLD4S`) não aparecem em nenhum slot alinhado, inclusive em uma busca direta pelas máscaras desses encodings. Foram reconhecidas 211 instruções de amostragem convencional (`TEX`/`TEXS`) em 81 dos 87 shaders Pixel; não encontrei os gathers que a correção Lemon v1.2.2 ajusta.

A cobertura do decoder não é total: ele reconheceu 7.891 de 10.134 words de 64 bits, deixando 2.243 words sem correspondência com os opcodes da tabela local. Não tratei esses words como inválidos. A busca direta específica pelos três encodings de gather também deu zero, mas não usei um disassembler NVIDIA independente nem traduzi os shaders completos. Portanto, a evidência limita a hipótese de gather para os shaders capturados, sem provar a causa visual nem cobrir variantes não reconhecidas.

## Correção do Lemon para efeitos colaterais de geometry shader

O Lemon v1.2.0 documenta “vertex spikes” e luzes piscando em *Marvel Ultimate Alliance 3* no Adreno. A análise upstream atribui o problema a efeitos colaterais repetidos por invocação de um geometry shader: o jogo faz compactação de triângulos com atomics e gera draw-indirect; o contador crescia três vezes, permitindo ler índices além da região escrita. A correção mais completa reclama cada primitiva usando um buffer scratch e uma geração por draw, protege atomics/writes sem suprimir a emissão normal dos vértices, desativa operações de subgroup no estágio geometry desses drivers e separa instâncias e sub-draws quando `PrimitiveId` recomeça [1] [2].

Esse patch é relevante para Adreno em geral, mas **não é evidência nem correção para os caches atuais de `Kill la Kill IF`**: os 87 registros Adreno e os 47 registros Mali guardados não têm geometry shader. Também não se deve aplicar um bloqueio genérico a todos os efeitos colaterais, pois shaders podem depender de side effects distintos por invocação.

## Outra correção Adreno: texture gather

O Lemon v1.2.2 e o PR do Eden #4565 documentam uma diferença de arredondamento subpixel nos `texture gather` entre Qualcomm/Turnip e Maxwell. Em TOTK, a seleção de texels vizinhos do limite de textura produzia grandes quadrados escuros nas sombras. Lemon ativa uma compensação apenas para esses drivers, e o upstream Eden adicionou Qualcomm Proprietary e Mesa Turnip à lista de GPUs que precisam dela [3] [4] [5].

Isso é uma correção de amostragem de textura, não de geometria duplicada. O shader compiler atualmente fixado pelo Strato não possui o campo/percurso `need_gather_subpixel_offset` usado pelo Lemon v1.2.2; portanto, não é suficiente apenas mudar a lista de drivers. Além disso, a análise estática atual não encontrou `TLD4` nos shaders desse cache. Não apliquei esse workaround; um port exigiria adaptar os backends SPIR-V e GLSL e validar a semântica em jogos que usem gathers.

## Otimização Lemon: cache de tradução de fragment shaders

O Lemon v1.2.0 relata que um fragment shader muito grande de *Splatoon 3* era traduzido uma vez por pipeline e podia consumir minutos de carregamento e muita memória. O cache novo o traduz uma vez por sessão, registrando as consultas ao ambiente do shader e reutilizando SPIR-V e metadados apenas quando essas respostas, as informações de runtime e as bindings são iguais. Ele mantém apenas traduções lentas (limiar de 30 ms) e serializa acessos concorrentes à mesma chave; o changelog mede a carga de aproximadamente 100 s para 70 s no AYN Odin 3 [1] [6].

No Strato, o `ShaderCache` já armazena o binário guest e o cache persistente restaura bundles de pipeline, mas o caminho de reconstrução volta a traduzir shaders guest para IR/SPIR-V. Há repetição de binários no cache de *Kill la Kill IF*, mas o volume é pequeno e os pipelines registrados carregaram em 246 ms. Assim, a otimização Lemon pode ser candidata para jogos com tradução comprovadamente lenta, mas estes dados não mostram que ela corrija o artefato ou melhore perceptivelmente este título. O ganho no Poco ainda não foi medido.

## Próxima verificação necessária

O Actions compilou o código e executou os testes Android e nativos no commit final [9], mas isso não valida o resultado visual no aparelho. O teste no POCO deve repetir a mesma cena com o driver do sistema, sem selecionar o Turnip v26.0.0 R8. Se o ghosting persistir, será preciso capturar nova imagem e log juntos para separar corrupção já presente no framebuffer guest de problemas na composição Android; não há evidência para aplicar os workarounds Lemon de geometry shader ou gather a esse jogo.

O artefato `Strato-Adreno-APK` desse run tem 28.529.006 bytes e SHA-256 `88b8ed0f2b1dcfffb38894c72e8d4ba2f0f041e690003b75d19ad31e8152d85d`; o ZIP interno foi verificado após o download.

## Referências

[1]: https://github.com/Ghael-V/Lemon-Project/releases/tag/v1.2.0 "Lemon Project v1.2.0"
[2]: https://github.com/Ghael-V/Lemon-Project/issues/7 "Lemon issue 7: Marvel Ultimate Alliance vertex explosion on Adreno"
[3]: https://github.com/Ghael-V/Lemon-Project/releases/tag/v1.2.2 "Lemon Project v1.2.2"
[4]: https://git.eden-emu.dev/eden-emu/eden/pulls/4565 "Eden PR 4565: Vulkan texture gather correction for TOTK"
[5]: https://github.com/Ghael-V/Lemon-Project/blob/v1.2.2/src/shader_recompiler/backend/spirv/emit_spirv_image.cpp "Lemon v1.2.2 SPIR-V image gather emitter"
[6]: https://github.com/Ghael-V/Lemon-Project/blob/v1.2.0/src/video_core/renderer_vulkan/vk_fragment_translation_cache.h "Lemon v1.2.0 fragment translation cache"
[7]: https://www.mi.com/ae-en/product/poco-x5-pro-5g/specs/ "Xiaomi POCO X5 Pro 5G specifications"
[8]: https://github.com/K11MCH1/AdrenoToolsDrivers/releases/tag/v26.0.0-rc08 "K11MCH1 Turnip v26.0.0 R8 release notes"
[9]: https://github.com/kuaan08/Strato/actions/runs/40611481048 "Successful final Strato Adreno Actions build"
