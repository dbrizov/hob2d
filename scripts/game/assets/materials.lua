DefineShader.Outline = {
    path = "shaders/outline",
    defaults = {
        tint = Color.white(),
        alpha_threshold = 0.1
    },
}

DefineShader.Psychedelic = {
    path = "shaders/psychedelic",
    defaults = {
        tint = Color.white(),
        alpha_threshold = 0.1
    },
}

DefineMaterial.Psychedelic = {
    shader = Shaders.Psychedelic,
}

DefineMaterial.WhiteOutline = {
    shader = Shaders.Outline,
    outline_color = Color.white(),
    outline_width = 2.0,
}

DefineMaterial.RedOutline = {
    shader = Shaders.Outline,
    outline_color = Color.red(),
    outline_width = 2.0,
}

DefineMaterial.GreenOutline = {
    shader = Shaders.Outline,
    outline_color = Color.green(),
    outline_width = 2.0,
}

DefineMaterial.BlueOutline = {
    shader = Shaders.Outline,
    outline_color = Color.blue(),
    outline_width = 2.0,
}
