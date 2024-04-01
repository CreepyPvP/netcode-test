-- vim.opt.errorformat = " %#%f(%l\\,%c):\\ %m"
vim.opt.makeprg = "build.bat"
vim.keymap.set("n", "<", "<cmd>tabnew term://run_server.bat<CR>" ..
                          "<cmd>vsplit><CR>" ..
                          "<cmd>term run_client.bat<CR>");

vim.keymap.set("n", "<A-c>", "<cmd>make<CR>");
vim.keymap.set("n", "<A-o>", "<cmd>!devenv client.exe<CR>");

