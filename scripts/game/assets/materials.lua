---@type MaterialParams.Psychedelic
DefineMaterial.Psychedelic = {
    shader = Shaders.Psychedelic,
}

---@type MaterialParams.Distort
DefineMaterial.Distort = {
    shader = Shaders.Distort,
    textures = {
        distort_tex = Textures.DistortNoise,
    },
}

---@type MaterialParams.Outline
DefineMaterial.WhiteOutline = {
    shader = Shaders.Outline,
    outline_color = Color.white(),
    outline_width = 2.0,
}

---@type MaterialParams.Outline
DefineMaterial.RedOutline = {
    shader = Shaders.Outline,
    outline_color = Color.red(),
    outline_width = 2.0,
}

---@type MaterialParams.Outline
DefineMaterial.GreenOutline = {
    shader = Shaders.Outline,
    outline_color = Color.green(),
    outline_width = 2.0,
}

---@type MaterialParams.Outline
DefineMaterial.BlueOutline = {
    shader = Shaders.Outline,
    outline_color = Color.blue(),
    outline_width = 2.0,
}
