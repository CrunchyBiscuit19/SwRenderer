#pragma once

class SwIResizable {
public:
    virtual void resize() = 0;
    virtual ~SwIResizable() = default;
};