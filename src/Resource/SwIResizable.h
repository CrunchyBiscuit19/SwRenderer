#pragma once

class SwIResizable {
public:
    bool mNeedResize{false};

    virtual void resize() = 0;
    virtual ~SwIResizable() = default;
};