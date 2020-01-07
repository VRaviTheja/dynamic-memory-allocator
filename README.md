# Dynamic-Memory-Allocator
• Implemented a dynamic memory allocator in c language.
• Free lists segregated by size class, using first-fit policy within each size class.
• Immediate coalescing of blocks on free with adjacent free blocks.
• Splitting to tackle splinters. Obfuscated block footers to improve security.
