/* Build-time Tailwind CSS v4.1.10 utility subset. No runtime dependency. */
const fs = require('node:fs');
const tw = require('tailwindcss');
const util = ['sr-only', 'flex', 'items-center', 'justify-between', 'gap-2', 'font-semibold', 'tabular-nums', 'min-w-0'];
tw.compile('@tailwind utilities;').then(c => {
 const css = c.build(util);
 if (!css.includes('.flex') || !css.includes('.tabular-nums')) throw Error('Tailwind compilation incomplete');
 fs.writeFileSync(__dirname+'/tailwind-compiled.css', '/*! Tailwind CSS v4.1.10 MIT, compiled offline */\n'+css);
 process.stdout.write('Compiled Tailwind CSS '+css.length+' bytes\n');
}).catch(e=>{console.error(e);process.exitCode=1});
