# LittlevGL + stb_truetype PoC

This is a simple PoC for getting LVGL work with stb_truetype to render TrueType fonts on ESP32.

## Tips for Asian fonts 

Asian fonts are usually huge, up to a few tens of MB. You may need to trim the font file before dumping it to ESP32. 

To do so, have a look at [fontTools](https://fonttools.readthedocs.io/en/latest/). Install it by `pip install fonttools`

Then, for example, go to `main/resource` directory, and run: 

```
pyftsubset wenquanyi.ttf --text-file=font_subset.txt --output-file=wqy.ttf
```

...where in this case:

- `wenquanyi.ttf` is a famous Chinese font, [Wen Quan Yi](http://wenq.org/wqy2/index.cgi?action=browse&id=Home&lang=en), in TrueType format
- `font_subset.txt` is the font subset I need for this demo project
- `wqy.ttf` is the trimmed TrueType font

Here's what it will be like:

![demo](main/resource/demo.jpg)

## To-do

1. Rasterize a lot of fonts may be a bit stressful for CPU and RAM. Therefore we probably need a KV database thing to handle font caching. One possible approach is to use ESP-IDF's NVS and prepare a separate NVS partition for fonts. Then store all rendered font bitmap cache to that partition.
2. Some heap allocations in stb_truetype can probably be avoided to prevent heap fragmentation.

## License

MIT or Apache-2.0